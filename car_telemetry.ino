#include <ArduinoJson.h>
#include "secrets.h"

static StaticJsonDocument<256> mqttDoc;
static const char *TOPIC_AVAIL = "car01/availability";
static const char *TOPIC_STATUS = "car01/status";
static const char *TOPIC_SIGNAL = "car01/signal";

// ===== ПІНИ =====
#define GSM_RX 18
#define GSM_TX 17
#define GSM_BAUD 115200

HardwareSerial GSM(2);
String line;

// ===== БАЗОВІ ФУНКЦІЇ AT =====

void flushGSM()
{
  while (GSM.available())
  {
    Serial.write(GSM.read());
  }
}

bool waitForResponse(const char *ok, const char *err = "ERROR", uint32_t timeout = 2000)
{
  String resp = "";
  uint32_t start = millis();

  while (millis() - start < timeout)
  {
    while (GSM.available())
    {
      char c = GSM.read();
      resp += c;
      Serial.write(c);
    }

    if (resp.indexOf(ok) != -1)
      return true;
    if (err && resp.indexOf(err) != -1)
      return false;

    delay(1); // даємо шанс RTOS / watchdog'у
  }

  return false;
}

bool sendATWait(const char *cmd, const char *ok = "OK", uint32_t timeout = 2000)
{
  flushGSM();
  Serial.print(">> ");
  Serial.println(cmd);
  GSM.println(cmd);
  return waitForResponse(ok, "ERROR", timeout);
}

bool waitForAT(uint32_t totalTimeout = 30000)
{
  uint32_t start = millis();
  while (millis() - start < totalTimeout)
  {
    if (sendATWait("AT", "OK", 1000))
      return true;
    delay(1000);
  }
  return false;
}

bool waitForSIM(uint32_t totalTimeout = 20000)
{
  uint32_t start = millis();
  while (millis() - start < totalTimeout)
  {
    flushGSM();
    Serial.println(">> AT+CPIN?");
    GSM.println("AT+CPIN?");

    String resp = "";
    uint32_t t = millis();
    while (millis() - t < 1500)
    {
      while (GSM.available())
      {
        char c = GSM.read();
        resp += c;
        Serial.write(c);
      }
    }
    if (resp.indexOf("READY") != -1)
      return true;
    delay(1000);
  }
  return false;
}

bool waitForNetwork(uint32_t totalTimeout = 45000)
{
  uint32_t start = millis();
  while (millis() - start < totalTimeout)
  {
    flushGSM();
    Serial.println(">> AT+CREG?");
    GSM.println("AT+CREG?");

    String resp = "";
    uint32_t t = millis();
    while (millis() - t < 1500)
    {
      while (GSM.available())
      {
        char c = GSM.read();
        resp += c;
        Serial.write(c);
      }
    }
    if (resp.indexOf("+CREG: 0,1") != -1 || resp.indexOf("+CREG: 0,5") != -1)
    {
      return true;
    }
    delay(1500);
  }
  return false;
}

// ===== MQTT НА БОЦІ МОДЕМУ =====

void mqttConnect()
{
  // чистимо попередній стан
  GSM.println("AT+CMQTTDISC=0,120");
  delay(500);
  GSM.println("AT+CMQTTREL=0");
  delay(500);
  GSM.println("AT+CMQTTSTOP");
  delay(1000);

  sendATWait("AT+CMQTTSTART");
  delay(1000);

  // SSL конфіг
  sendATWait("AT+CSSLCFG=\"sslversion\",0,4");
  sendATWait("AT+CSSLCFG=\"authmode\",0,0"); // без перевірки серта

  // створюємо клієнта
  char buf[64];
  snprintf(buf, sizeof(buf), "AT+CMQTTACCQ=0,\"%s\",1", MQTT_CLIENT);
  sendATWait(buf);

  sendATWait("AT+CMQTTSSLCFG=0,0");

  // CONNECT
  char cmd[160];
  snprintf(cmd, sizeof(cmd),
           "AT+CMQTTCONNECT=0,\"tcp://%s:%d\",60,1,\"%s\",\"%s\"",
           MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASS);
  Serial.println(">> CONNECT");
  GSM.println(cmd);
  waitForResponse("OK", "ERROR", 10000); // дивимось у Serial, що поверне
}

void mqttSend(const String &topic, const String &payload, bool retain = false)
{
  GSM.printf("AT+CMQTTTOPIC=0,%d\r\n", topic.length());
  delay(200);
  GSM.print(topic);
  delay(200);

  GSM.printf("AT+CMQTTPAYLOAD=0,%d\r\n", payload.length());
  delay(200);
  GSM.print(payload);
  delay(200);

  if (retain)
    GSM.println("AT+CMQTTPUB=0,1,60,1");
  else
    GSM.println("AT+CMQTTPUB=0,1,60");
  waitForResponse("OK", "ERROR", 5000);
}

template <typename Builder>
void mqttPublishJson(const String &topic, Builder builder, bool retain = false)
{
  mqttDoc.clear();
  JsonObject root = mqttDoc.to<JsonObject>();

  builder(root);

  String payload;
  serializeJson(mqttDoc, payload);

  mqttSend(topic, payload, retain);
}

void sendSignalFromRssi(int rssi)
{
  // 99 означає, що сигнал не виміряно [web:75]
  if (rssi == 99)
    return;

  int dbm = -113 + (rssi * 2);

  mqttPublishJson(TOPIC_SIGNAL, [rssi, dbm](JsonObject &root)
                  {
    root["rssi"] = rssi;
    root["dbm"]  = dbm; }, false);
}

// ===== SETUP / LOOP =====

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== STEP 1: MODEM + MQTT over AT ===");

  GSM.begin(GSM_BAUD, SERIAL_8N1, GSM_RX, GSM_TX);
  delay(3000);

  if (!waitForAT())
  {
    Serial.println("ERROR: modem does not answer AT");
    return;
  }
  Serial.println("MODEM OK");

  sendATWait("ATE0");
  sendATWait("AT+CMEE=2");

  if (!waitForSIM())
  {
    Serial.println("ERROR: SIM not ready");
    return;
  }
  Serial.println("SIM READY");

  if (!waitForNetwork())
  {
    Serial.println("ERROR: network not registered");
    return;
  }
  Serial.println("NETWORK READY");

  mqttConnect();
  delay(2000);

  // availability (retain, щоб HA одразу бачив стан після рестарту)
  mqttPublishJson(TOPIC_AVAIL, [](JsonObject &root)
                  { root["state"] = "online"; }, true);

  // простий статус
  mqttPublishJson(TOPIC_STATUS, [](JsonObject &root)
                  {
    root["state"]   = "booted";
    root["version"] = "0.1-gsm-mqtt"; }, false);

  // тестовий publish
  mqttPublishJson("car01/test", [](JsonObject &root)
                  { root["ok"] = true; }, false);

  Serial.println("SETUP DONE");
}

unsigned long lastSignalReq = 0;

void loop()
{
  // читаємо відповіді з модема
  while (GSM.available())
  {
    char c = GSM.read();
    Serial.write(c);

    if (c == '\n')
    {
      line.trim();

      // парсинг +CSQ: 24,99
      if (line.startsWith("+CSQ:"))
      {
        int comma = line.indexOf(',');
        int rssi = line.substring(6, comma).toInt();
        sendSignalFromRssi(rssi);
      }

      line = "";
    }
    else
    {
      line += c;
    }
  }

  // кожні 10 секунд просимо CSQ
  if (millis() - lastSignalReq > 10000UL)
  {
    lastSignalReq = millis();
    GSM.println("AT+CSQ");
  }
}