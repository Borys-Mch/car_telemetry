/*
 * Car Telemetry - MQTT + FreeRTOS tasks
 * ESP32-S3 + A7670E (UART1 GPIO18/17) + ELM327 (UART0 GPIO44/43)
 *
 * Бібліотеки: ArduinoJson
 * Arduino IDE → Tools → USB CDC On Boot: Enabled
 */

#include <ArduinoJson.h>
#include "esp_task_wdt.h"
#include "secrets.h"

// ===== ПІНИ =================================================
#define GSM_RX 18
#define GSM_TX 17
#define GSM_BAUD 115200
#define PWK_PIN 4
#define BTN_PIN 5

// ===== UART =================================================
HardwareSerial GSM(2);

// буфер для парсингу
String line = "";

// таймер
unsigned long lastSend = 0;

void sendAT(const char *cmd)
{
  GSM.println(cmd);
}

void mqttConnect()
{
  sendAT("AT+CMQTTSTART");
  delay(1000);

  sendAT("AT+CSSLCFG=\"sslversion\",0,4");
  delay(200);

  sendAT("AT+CSSLCFG=\"authmode\",0,0"); // без перевірки
  delay(200);

  GSM.printf("AT+CMQTTACCQ=0,\"%s\",1\r\n", MQTT_CLIENT_ID);
  delay(500);

  sendAT("AT+CMQTTSSLCFG=0,0");
  delay(200);

  GSM.printf(
      "AT+CMQTTCONNECT=0,\"tcp://%s:%d\",60,1,\"%s\",\"%s\"\r\n",
      MQTT_HOST,
      MQTT_PORT,
      MQTT_USER,
      MQTT_PASS);

  delay(2000);
}

void mqttPublish(int rssi)
{
  int dbm = -113 + (rssi * 2);

  String payload = "{\"rssi\":" + String(rssi) + ",\"dbm\":" + String(dbm) + "}";
  String topic = "home/car/telemetry";

  GSM.printf("AT+CMQTTTOPIC=0,%d\r\n", topic.length());
  delay(100);
  GSM.print(topic);
  delay(100);

  GSM.printf("AT+CMQTTPAYLOAD=0,%d\r\n", payload.length());
  delay(100);
  GSM.print(payload);
  delay(100);

  sendAT("AT+CMQTTPUB=0,1,60");
}

void requestSignal()
{
  sendAT("AT+CSQ");
}

void setup()
{
  Serial.begin(115200);
  GSM.begin(GSM_BAUD, SERIAL_8N1, GSM_RX, GSM_TX);

  delay(3000);

  sendAT("AT");
  delay(1000);

  mqttConnect();
}

void loop()
{
  // ЄДИНЕ місце де читається модем
  while (GSM.available())
  {
    char c = GSM.read();
    Serial.write(c);

    if (c == '\n')
    {
      line.trim();

      // парсимо сигнал
      if (line.startsWith("+CSQ:"))
      {
        int comma = line.indexOf(',');
        int rssi = line.substring(6, comma).toInt();

        if (rssi != 99)
        {
          mqttPublish(rssi);
        }
      }

      line = "";
    }
    else
    {
      line += c;
    }
  }

  // кожні 10 сек запит сигналу
  if (millis() - lastSend > 10000)
  {
    lastSend = millis();
    requestSignal();
  }
}