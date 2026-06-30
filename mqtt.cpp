#include "mqtt.h"
#include "modem.h"
#include "config.h"
#include "secrets.h"

// ─── стан з'єднання ───────────────────────────────────────────────────────────
bool mqttConnected = false;

// час останнього успішного publish (підтверджений "+CMQTTPUB:0,0")
static unsigned long lastSuccessfulPub = 0;

// throttle між AT-командами publish
static unsigned long lastMQTTsend = 0;

static StaticJsonDocument<512> discDoc;

// ─── внутрішня публікація ─────────────────────────────────────────────────────
// Неблокуюча: надсилає AT-команди і виходить.
// Результат +CMQTTPUB обробляється в mqttHandleURC() з loop().
static void mqttSendRaw(const String &topic, const String &payload, bool retain)
{
  if (!mqttConnected)
  {
    Serial.println("[MQTT] Skip send – not connected");
    return;
  }

  // throttle: мінімум 1500 мс між publish
  unsigned long now = millis();
  if (now - lastMQTTsend < 1500)
    delay(1500 - (now - lastMQTTsend));
  lastMQTTsend = millis();

  GSM.printf("AT+CMQTTTOPIC=0,%d\r\n", topic.length());
  delay(200);
  GSM.print(topic);
  delay(100);

  GSM.printf("AT+CMQTTPAYLOAD=0,%d\r\n", payload.length());
  delay(200);
  GSM.print(payload);
  delay(100);

  if (retain)
    GSM.println("AT+CMQTTPUB=0,1,60,1");
  else
    GSM.println("AT+CMQTTPUB=0,1,60");

  // НЕ чекаємо тут — +CMQTTPUB прийде асинхронно і
  // буде оброблений в mqttHandleURC() → loop()
}

// ─── connect ──────────────────────────────────────────────────────────────────
void mqttConnect()
{
  Serial.println("[MQTT] Starting connect sequence...");
  mqttConnected = false;

  // м'який reset стеку
  GSM.println("AT+CMQTTDISC=0,120");
  delay(1000);
  GSM.println("AT+CMQTTREL=0");
  delay(500);
  GSM.println("AT+CMQTTSTOP");
  delay(2000);

  // старт стеку
  // модем відповідає "OK" одразу, але "+CMQTTSTART: 0" приходить пізніше
  GSM.println("AT+CMQTTSTART");
  {
    unsigned long t = millis();
    String r = "";
    bool started = false;
    while (millis() - t < 10000 && !started)
    {
      while (GSM.available())
      {
        char c = GSM.read();
        Serial.write(c);
        if (c == '\n')
        {
          r.trim();
          if (r.startsWith("+CMQTTSTART"))
            started = true;
          r = "";
        }
        else
          r += c;
      }
    }
    if (!started)
    {
      Serial.println("[MQTT] CMQTTSTART timeout");
      return;
    }
  }
  delay(300);

  sendATWait("AT+CSSLCFG=\"sslversion\",0,4", "OK", 2000);
  sendATWait("AT+CSSLCFG=\"authmode\",0,0", "OK", 2000);

  // client id
  char buf[64];
  snprintf(buf, sizeof(buf), "AT+CMQTTACCQ=0,\"%s\",1", MQTT_CLIENT_ID);
  if (!sendATWait(buf, "OK", 2000))
  {
    Serial.println("[MQTT] CMQTTACCQ failed");
    return;
  }

  sendATWait("AT+CMQTTSSLCFG=0,0", "OK", 2000);

  // CONNECT
  char cmd[256];
  snprintf(cmd, sizeof(cmd),
           "AT+CMQTTCONNECT=0,\"tcp://%s:%d\",60,1,\"%s\",\"%s\"",
           MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASS);

  // LWT topic
  GSM.printf("AT+CMQTTWILLTOPIC=0,%d\r\n", strlen(MQTT_TOPIC_AVAILABILITY));
  delay(200);
  GSM.print(MQTT_TOPIC_AVAILABILITY);
  delay(200);

  // LWT payload
  GSM.printf("AT+CMQTTWILLMSG=0,%d,1\r\n", strlen("offline")); // 1 = QoS1
  delay(200);
  GSM.print("offline");
  delay(200);

  Serial.println("[MQTT] >> CONNECT");
  GSM.println(cmd);

  // Модем спочатку відповідає "OK", потім асинхронно "+CMQTTCONNECT:0,<code>"
  // Тому читаємо всі рядки протягом 15 секунд і шукаємо саме +CMQTTCONNECT
  unsigned long t = millis();
  String resp = "";
  bool connected = false;
  bool gotResult = false;

  while (millis() - t < 15000 && !gotResult)
  {
    while (GSM.available())
    {
      char c = GSM.read();
      Serial.write(c);
      if (c == '\n')
      {
        resp.trim();
        if (resp.startsWith("+CMQTTCONNECT:"))
        {
          gotResult = true;
          // формат: +CMQTTCONNECT: 0,0  (пробіл після : — нормально)
          // витягуємо код після останньої коми
          int comma = resp.lastIndexOf(',');
          if (comma != -1)
          {
            int code = resp.substring(comma + 1).toInt();
            if (code == 0)
            {
              connected = true;
            }
            else
            {
              Serial.println("[MQTT] CONNECT rejected, code=" + String(code));
            }
          }
        }
        resp = "";
      }
      else
        resp += c;
    }
  }

  if (connected)
  {
    mqttConnected = true;
    lastSuccessfulPub = millis();
    Serial.println("[MQTT] Connected OK");
    mqttSendRaw(MQTT_TOPIC_AVAILABILITY, "online", true);
  }
  else
  {
    if (!gotResult)
      Serial.println("[MQTT] Connect TIMEOUT (no +CMQTTCONNECT received)");
    else
      Serial.println("[MQTT] Connect FAILED");
  }
}

// ─── reconnect (повний цикл) ──────────────────────────────────────────────────
void mqttReconnect()
{
  Serial.println("[MQTT] === Reconnect ===");

  mqttConnect();

  if (!mqttConnected)
    return;

  delay(1500);
  mqttSubscribeCmd();
  delay(1000);

  // Discovery повторно не потрібна при кожному reconnect —
  // брокер зберігає retain-повідомлення.
  // Але якщо брокер перезапускався (retain втрачено) — розкоментуй:
  //
  // mqttPublishSignalDiscovery();      delay(500);
  // mqttPublishCallStatusDiscovery();  delay(500);
  // mqttSendIncomingDiscovery();       delay(500);
  // mqttPublishGateButtonDiscovery();  delay(500);
  // mqttPublishHangupButtonDiscovery();delay(500);
  // mqttSendDiscoveryGps();            delay(500);

  Serial.println("[MQTT] Reconnect done");
}

// ─── watchdog: викликати з loop() раз на ~30 сек ─────────────────────────────
void mqttWatchdog()
{
  static unsigned long lastCheck = 0;
  unsigned long now = millis();

  if (now - lastCheck < 30000UL)
    return;
  lastCheck = now;

  // 1) явний флаг: URC прийшов і скинув mqttConnected
  if (!mqttConnected)
  {
    Serial.println("[MQTT] Watchdog: not connected, reconnecting...");
    mqttReconnect();
    return;
  }

  // 2) heartbeat: якщо >3 хв без успішного publish — вважаємо мертвим
  //    (кожні 10 сек надсилається CSQ -> mqttSendSignal, тобто ~6 publish/хв у нормі)
  if (now - lastSuccessfulPub > 180000UL)
  {
    Serial.println("[MQTT] Watchdog: heartbeat timeout, reconnecting...");
    mqttConnected = false;
    mqttReconnect();
  }
}

// ─── обробник URC від модема ──────────────────────────────────────────────────
// Викликати з loop() для кожного отриманого рядка від GSM
void mqttHandleURC(const String &line)
{
  // ── підтвердження publish ────────────────────────────────────────────────
  if (line.startsWith("+CMQTTPUB:"))
  {
    // формат: +CMQTTPUB: 0,0  або  +CMQTTPUB:0,0
    int comma = line.lastIndexOf(',');
    if (comma != -1)
    {
      int code = line.substring(comma + 1).toInt();
      if (code == 0)
      {
        lastSuccessfulPub = millis();
        // Serial.println("[MQTT] Pub OK");  // розкоментуй для дебагу
      }
      else
      {
        Serial.println("[MQTT] Pub error code=" + String(code));
        mqttConnected = false;
      }
    }
    return;
  }

  // ── втрата з'єднання ────────────────────────────────────────────────────
  if (line.startsWith("+CMQTTCONNLOST") ||
      line.startsWith("+CMQTTNONET") ||
      (line.startsWith("+CMQTTERROR") && mqttConnected))
  {
    Serial.println("[MQTT] URC: connection lost -> " + line);
    mqttConnected = false;
  }
}

// ─── публічне API ─────────────────────────────────────────────────────────────
void mqttPublishDiscovery(const String &topic, HaDiscoveryBuilder builder)
{
  discDoc.clear();
  JsonObject root = discDoc.to<JsonObject>();
  builder(root);

  String payload;
  serializeJson(discDoc, payload);
  mqttSendRaw(topic, payload, true);
}

void mqttSendSignal(int rssi)
{
  if (rssi == 99)
    return;

  int dbm = -113 + (rssi * 2);
  int prs = constrain(map(rssi, 0, 31, 0, 100), 0, 100);
  String payload = "{\"rssi\":" + String(rssi) + ",\"dbm\":" + String(dbm) + ",\"prs\":" + String(prs) + "}";
  mqttSendRaw(MQTT_TOPIC_TELEMETRY, payload, false);
}

void mqttPublishSignalDiscovery()
{
  const char *discTopic = "homeassistant/sensor/car_signal/config";
  mqttPublishDiscovery(discTopic, [](JsonObject &root)
                       {
    root["name"]                    = "Modem Signal";
    root["state_topic"]             = MQTT_TOPIC_TELEMETRY;
    root["state_class"]             = "measurement";
    root["unit_of_measurement"]     = "%";
    root["value_template"]          = "{{ value_json.prs }}";
    root["json_attributes_topic"]   = MQTT_TOPIC_TELEMETRY;
    root["unique_id"]               = "modem_signal";
    root["availability_topic"]      = MQTT_TOPIC_AVAILABILITY;
    root["payload_available"]       = "online";
    root["payload_not_available"]   = "offline";

    JsonObject dev = root["device"].to<JsonObject>();
    dev["name"]           = MQTT_CLIENT_NAME;
    dev["identifiers"][0] = MQTT_CLIENT_ID;
    dev["model"]          = MQTT_CLIENT_MODEL;
    dev["manufacturer"]   = MQTT_CLIENT_MANUFACTURER; });
}

void mqttSendCallStatus(const String &number, const String &status)
{
  String payload = "{\"number\":\"" + number + "\",\"state\":\"" + status + "\"}";
  mqttSendRaw(MQTT_TOPIC_STATUS, payload, true);
}

void mqttPublishCallStatusDiscovery()
{
  const char *discTopic = "homeassistant/sensor/car_call_status/config";
  mqttPublishDiscovery(discTopic, [](JsonObject &root)
                       {
    root["name"]                  = "Call Status";
    root["state_topic"]           = MQTT_TOPIC_STATUS;
    root["value_template"]        = "{{ value_json.state }}";
    root["unique_id"]             = "call_status";
    root["icon"]                  = "mdi:phone";
    root["availability_topic"]    = MQTT_TOPIC_AVAILABILITY;
    root["payload_available"]     = "online";
    root["payload_not_available"] = "offline";

    JsonObject dev = root["device"].to<JsonObject>();
    dev["name"]           = MQTT_CLIENT_NAME;
    dev["identifiers"][0] = MQTT_CLIENT_ID; });
}

void mqttSendIncomingDiscovery()
{
  const char *discTopic = "homeassistant/sensor/car_incoming_call/config";
  mqttPublishDiscovery(discTopic, [](JsonObject &root)
                       {
    root["name"]                  = "Incoming Call";
    root["state_topic"]           = MQTT_TOPIC_STATUS;
    root["value_template"]        = "{{ value_json.number }}";
    root["json_attributes_topic"] = MQTT_TOPIC_STATUS;
    root["unique_id"]             = "car_incoming_call";
    root["icon"]                  = "mdi:phone-incoming";
    root["availability_topic"]    = MQTT_TOPIC_AVAILABILITY;
    root["payload_available"]     = "online";
    root["payload_not_available"] = "offline";

    JsonObject dev = root["device"].to<JsonObject>();
    dev["name"]           = MQTT_CLIENT_NAME;
    dev["identifiers"][0] = MQTT_CLIENT_ID; });
}

void mqttPublishGateButtonDiscovery()
{
  const char *discTopic = "homeassistant/button/car_gate/config";
  mqttPublishDiscovery(discTopic, [](JsonObject &root)
                       {
    root["name"]                  = "Gate Open";
    root["command_topic"]         = MQTT_TOPIC_SUBSCRIBE;
    root["payload_press"]         = "gate";
    root["unique_id"]             = "car_gate_btn";
    root["icon"]                  = "mdi:boom-gate-outline";
    root["availability_topic"]    = MQTT_TOPIC_AVAILABILITY;
    root["payload_available"]     = "online";
    root["payload_not_available"] = "offline";

    JsonObject dev = root["device"].to<JsonObject>();
    dev["name"]           = MQTT_CLIENT_NAME;
    dev["identifiers"][0] = MQTT_CLIENT_ID; });
}

void mqttPublishHangupButtonDiscovery()
{
  const char *discTopic = "homeassistant/button/car_call_end/config";
  mqttPublishDiscovery(discTopic, [](JsonObject &root)
                       {
    root["name"]                  = "End Call";
    root["command_topic"]         = MQTT_TOPIC_SUBSCRIBE;
    root["payload_press"]         = "hangup";
    root["unique_id"]             = "car_call_end_btn";
    root["icon"]                  = "mdi:phone-hangup";
    root["availability_topic"]    = MQTT_TOPIC_AVAILABILITY;
    root["payload_available"]     = "online";
    root["payload_not_available"] = "offline";

    JsonObject dev = root["device"].to<JsonObject>();
    dev["name"]           = MQTT_CLIENT_NAME;
    dev["identifiers"][0] = MQTT_CLIENT_ID; });
}

void mqttSendGps(float lat, float lon, int sats, const String &status, bool hasFix)
{
  String payload = "{";
  if (hasFix)
  {
    payload += "\"latitude\":" + String(lat, 6) + ",";
    payload += "\"longitude\":" + String(lon, 6) + ",";
    payload += "\"sat\":" + String(sats) + ",";
  }
  else
  {
    payload += "\"latitude\":null,";
    payload += "\"longitude\":null,";
    payload += "\"sat\":null,";
  }
  payload += "\"status\":\"" + status + "\"";
  payload += "}";
  mqttSendRaw(MQTT_TOPIC_GPS, payload, true);
}

void mqttSendDiscoveryGps()
{
  const char *discTopic = "homeassistant/device_tracker/cargps/config";
  mqttPublishDiscovery(discTopic, [](JsonObject &root)
                       {
    root["name"]                  = "Car GPS";
    root["state_topic"]           = MQTT_TOPIC_GPS;
    root["json_attributes_topic"] = MQTT_TOPIC_GPS;
    root["unique_id"]             = "car_gpstracker";
    root["source_type"]           = "gps";
    root["icon"]                  = "mdi:satellite-variant";
    root["availability_topic"]    = MQTT_TOPIC_AVAILABILITY;
    root["payload_available"]     = "online";
    root["payload_not_available"] = "offline";

    JsonObject dev = root["device"].to<JsonObject>();
    dev["name"]           = MQTT_CLIENT_NAME;
    dev["identifiers"][0] = MQTT_CLIENT_ID; });
}

void mqttSubscribeCmd()
{
  const char *topic = MQTT_TOPIC_SUBSCRIBE;
  GSM.printf("AT+CMQTTSUB=0,%d,1\r\n", strlen(topic));
  delay(300);
  GSM.print(topic);
  delay(300);
  Serial.println("[MQTT] Subscribed to car/cmd");
}