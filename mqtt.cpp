#include "mqtt.h"
#include "modem.h"
#include "config.h"
#include "secrets.h"

// будемо використовувати той самий lastMQTTsend, що й раніше,
// але поки зробимо локальну змінну тут:
static unsigned long lastMQTTsend = 0;
static StaticJsonDocument<512> discDoc;

void mqttConnect()
{
  // м’який reset MQTT-клієнта модема
  GSM.println("AT+CMQTTDISC=0,120");
  delay(500);

  GSM.println("AT+CMQTTREL=0");
  delay(500);

  GSM.println("AT+CMQTTSTOP");
  delay(1000);

  // старт MQTT-стеку
  sendATWait("AT+CMQTTSTART", "OK", 10000);
  sendATWait("AT+CSSLCFG=\"sslversion\",0,4", "OK", 2000);
  sendATWait("AT+CSSLCFG=\"authmode\",0,0", "OK", 2000); // без перевірки сертифіката

  // client id
  char buf[64];
  snprintf(buf, sizeof(buf),
           "AT+CMQTTACCQ=0,\"%s\",1",
           MQTT_CLIENT_ID);
  sendATWait(buf, "OK", 2000);

  // прив'язка SSL-профілю 0 до MQTT-клієнта 0
  sendATWait("AT+CMQTTSSLCFG=0,0", "OK", 2000);

  // CONNECT
  char cmd[160];
  snprintf(cmd, sizeof(cmd),
           "AT+CMQTTCONNECT=0,\"tcp://%s:%d\",60,1,\"%s\",\"%s\"",
           MQTT_HOST,
           MQTT_PORT,
           MQTT_USER,
           MQTT_PASS);

  Serial.println(">> MQTT CONNECT");
  GSM.println(cmd);

  // просто чекаємо OK або ERROR, як у твоєму waitForResponse
  if (!sendATWait("", "OK", 10000))
  {
    Serial.println("MQTT CONNECT may have failed (no OK seen)");
  }
  else
  {
    Serial.println("MQTT CONNECT sent");
  }
}

static void mqttSendRaw(const String &topic, const String &payload, bool retain)
{
  // throttle як у твоєму коді
  if (millis() - lastMQTTsend < 1500)
  {
    delay(1500 - (millis() - lastMQTTsend));
  }
  lastMQTTsend = millis();

  GSM.printf("AT+CMQTTTOPIC=0,%d\r\n", topic.length());
  delay(300);
  GSM.print(topic);
  delay(300);

  GSM.printf("AT+CMQTTPAYLOAD=0,%d\r\n", payload.length());
  delay(300);
  GSM.print(payload);
  delay(300);

  if (retain)
    GSM.println("AT+CMQTTPUB=0,1,60,1");
  else
    GSM.println("AT+CMQTTPUB=0,1,60");

  delay(500);
}

void mqttPublishDiscovery(const String &topic, HaDiscoveryBuilder builder)
{
  discDoc.clear();
  JsonObject root = discDoc.to<JsonObject>();

  builder(root); // користувацький заповнювач

  String payload;
  serializeJson(discDoc, payload);

  mqttSendRaw(topic, payload, true); // discovery завжди з retain
}

void mqttSendSignal(int rssi)
{
  if (rssi == 99)
    return; // невідомий рівень

  int dbm = -113 + (rssi * 2);
  String payload = "{\"rssi\":" + String(rssi) +
                   ",\"dbm\":" + String(dbm) + "}";

  mqttSendRaw("car/telemetry", payload, false);
}

void mqttSendCallStatus(const String &status)
{
  String payload = "{\"call\":\"" + status + "\"}";
  mqttSendRaw("car/status", payload, true); // retain, щоб HA завжди бачила останній стан
}

void mqttPublishSignalDiscovery()
{
  const char *discTopic = "homeassistant/sensor/car_signal/config";
  mqttPublishDiscovery(discTopic, [](JsonObject &root)
                       {
    root["name"]        = "Modem Signal";
    root["state_topic"] = "car/telemetry";
    root["unit_of_measurement"] = "dBm";
    root["value_template"]      = "{{ value_json.dbm }}";
    root["unique_id"]   = "modem_signal";

    JsonObject dev = root["device"].to<JsonObject>();
    dev["identifiers"][0] = "car_info";
    dev["name"]           = "Car Info";
    dev["model"]          = "ESP32-S3 + A7670E + ELM327";
    dev["manufacturer"]   = "DEREN"; });
}

void mqttPublishCallStatusDiscovery()
{
  const char *discTopic = "homeassistant/sensor/car_call_status/config";

  mqttPublishDiscovery(discTopic, [](JsonObject &root)
                       {
    root["name"]        = "Car Call Status";
    root["state_topic"] = "car/status";
    root["value_template"] = "{{ value_json.call }}";
    root["unique_id"]   = "car_call_status";
    root["icon"]        = "mdi:phone";

    JsonObject dev = root["device"].to<JsonObject>();
    dev["identifiers"][0] = "car_info";
    dev["name"]           = "Car Info"; });
}

void mqttSendIncomingDiscovery()
{
  const char *discTopic = "homeassistant/sensor/car_incoming_call/config";

  mqttPublishDiscovery(discTopic, [](JsonObject &root)
                       {
    root["name"]                  = "Incoming Call";
    root["state_topic"]           = "car/incoming";
    root["value_template"]        = "{{ value_json.number }}";
    root["json_attributes_topic"] = "car/incoming";
    root["unique_id"]             = "car_incoming_call";
    root["icon"]                  = "mdi:phone-incoming";

    JsonObject dev = root["device"].to<JsonObject>();
    dev["identifiers"][0] = "car_info";
    dev["name"]           = "Car Info"; });
}

void mqttSendIncomingCall(const String &number)
{
  String payload = "{";
  payload += "\"number\":\"" + number + "\",";
  payload += "\"state\":\"ringing\"";
  payload += "}";

  mqttSendRaw("car/incoming", payload, true);
}

void mqttClearIncomingCall()
{
  String payload = "{";
  payload += "\"number\":\"\",";
  payload += "\"state\":\"idle\"";
  payload += "}";

  mqttSendRaw("car/incoming", payload, true);
}

void mqttPublishGateButtonDiscovery()
{
  const char *discTopic = "homeassistant/button/car_gate/config";

  mqttPublishDiscovery(discTopic, [](JsonObject &root)
                       {
    root["name"]          = "Gate Open";
    root["command_topic"] = "car/cmd";
    root["payload_press"] = "gate";
    root["unique_id"]     = "car_gate_btn";
    root["icon"]          = "mdi:boom-gate-outline";

    JsonObject dev = root["device"].to<JsonObject>();
    dev["identifiers"][0] = "car_info";
    dev["name"]           = "Car Info"; });
}

void mqttPublishHangupButtonDiscovery()
{
  const char *discTopic = "homeassistant/button/car_call_end/config";

  mqttPublishDiscovery(discTopic, [](JsonObject &root)
                       {
    root["name"]          = "End Call";
    root["command_topic"] = "car/cmd";
    root["payload_press"] = "hangup";
    root["unique_id"]     = "car_call_end_btn";
    root["icon"]          = "mdi:phone-hangup";

    JsonObject dev = root["device"].to<JsonObject>();
    dev["identifiers"][0] = "car_info";
    dev["name"]           = "Car Info"; });
}

void mqttSubscribeCmd()
{
  const char *topic = "car/cmd";

  // Формуємо AT+CMQTTSUB
  GSM.printf("AT+CMQTTSUB=0,%d,1\r\n", strlen(topic));
  delay(100);
  GSM.print(topic);
  delay(100);
}