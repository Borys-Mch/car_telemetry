#include "mqtt_at.h"
#include "modem.h"
#include "secrets.h"

StaticJsonDocument<256> mqttDoc;

const char *TOPIC_AVAIL = "car01/availability";
const char *TOPIC_STATUS = "car01/status";
const char *TOPIC_SIGNAL = "car01/signal";

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

bool mqttInit()
{
  mqttConnectRaw();

  mqttPublishJson(TOPIC_AVAIL, [](JsonObject &root)
                  { root["state"] = "online"; }, true);

  mqttPublishJson(TOPIC_STATUS, [](JsonObject &root)
                  {
    root["state"]   = "booted";
    root["version"] = "0.1-gsm-mqtt"; }, false);

  return true;
}