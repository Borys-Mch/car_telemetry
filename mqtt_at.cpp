#include "mqtt_at.h"
#include "modem.h"
#include "secrets.h"

StaticJsonDocument<256> mqttDoc;

const char *TOPIC_AVAIL = "car/availability";
const char *TOPIC_STATUS = "car/status";
const char *TOPIC_SIGNAL = "car/signal";

static const char *DISCOVERY_PREFIX = "homeassistant";
static const char *DEVICE_ID = "car_telemetry";

static void addDeviceBlock(JsonObject &dev)
{
  dev["identifiers"][0] = DEVICE_ID;
  dev["name"] = "Car Telemetry";
  dev["model"] = "ESP32-S3 + A7670E";
  dev["manufacturer"] = "DEREN";
}

static void mqttConnectRaw()
{
  GSM.println("AT+CMQTTDISC=0,120");
  delay(500);
  GSM.println("AT+CMQTTREL=0");
  delay(500);
  GSM.println("AT+CMQTTSTOP");
  delay(1000);

  sendATWait("AT+CMQTTSTART", "OK", 10000);
  sendATWait("AT+CSSLCFG=\"sslversion\",0,4");
  sendATWait("AT+CSSLCFG=\"authmode\",0,0");

  char buf[64];
  snprintf(buf, sizeof(buf),
           "AT+CMQTTACCQ=0,\"%s\",1", MQTT_CLIENT);
  sendATWait(buf);

  sendATWait("AT+CMQTTSSLCFG=0,0");

  char cmd[160];
  snprintf(cmd, sizeof(cmd),
           "AT+CMQTTCONNECT=0,\"tcp://%s:%d\",60,1,\"%s\",\"%s\"",
           MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASS);
  GSM.println(cmd);
  waitForResponse("OK", "ERROR", 10000);
}

void mqttSend(const String &topic, const String &payload, bool retain)
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

static void publishSignalDiscovery()
{
  String topic = String(DISCOVERY_PREFIX) +
                 "/sensor/" +
                 DEVICE_ID +
                 "_signal/config";

  mqttPublishJson(topic, [](JsonObject &root)
                  {
    root["name"]            = "Car Modem Signal";
    root["unique_id"]       = String(DEVICE_ID) + "_signal";
    root["state_topic"]     = TOPIC_SIGNAL;
    root["unit_of_measurement"] = "dBm";
    root["value_template"]  = "{{ value_json.dbm }}";
    root["device_class"]    = "signal_strength";
    root["availability_topic"] = TOPIC_AVAIL;

    JsonObject dev = root["device"].to<JsonObject>();
    addDeviceBlock(dev); }, true);
}

static void publishStatusDiscovery()
{
  String topic = String(DISCOVERY_PREFIX) +
                 "/sensor/" +
                 DEVICE_ID +
                 "_status/config";

  mqttPublishJson(topic, [](JsonObject &root)
                  {
    root["name"]        = "Car Telemetry Status";
    root["unique_id"]   = String(DEVICE_ID) + "_status";
    root["state_topic"] = TOPIC_STATUS;
    root["value_template"] = "{{ value_json.state }}";
    root["icon"]        = "mdi:car-connected";
    root["availability_topic"] = TOPIC_AVAIL;

    JsonObject dev = root["device"].to<JsonObject>();
    addDeviceBlock(dev); }, true);
}

void mqttSetAvailabilityOnline()
{
  mqttSend(TOPIC_AVAIL, "online", true);
}

void mqttSetAvailabilityOffline()
{
  mqttSend(TOPIC_AVAIL, "offline", true);
}

bool mqttInit()
{
  mqttConnectRaw();
  mqttSetAvailabilityOnline();

  mqttPublishJson(TOPIC_STATUS, [](JsonObject &root)
                  {
    root["state"]   = "booted";
    root["version"] = "0.1-gsm-mqtt"; }, false);

  // нове:
  publishSignalDiscovery();
  publishStatusDiscovery();

  return true;
}