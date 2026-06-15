#include "modem.h"
#include "mqtt_at.h"
#include "gps_at.h"

String line;
unsigned long lastSignalReq = 0;

void sendSignalFromRssi(int rssi)
{
  if (rssi == 99)
    return;
  int dbm = -113 + (rssi * 2);
  mqttPublishJson(TOPIC_SIGNAL, [rssi, dbm](JsonObject &root)
                  {
    root["rssi"] = rssi;
    root["dbm"]  = dbm; }, false);
}

void setup()
{
  if (!modemInit())
  {
    delay(10000);
    ESP.restart();
  }

  if (!mqttInit())
  {
    delay(10000);
    ESP.restart();
  }
  gpsInit();
}

void loop()
{
  while (GSM.available())
  {
    char c = GSM.read();
    Serial.write(c);

    if (c == '\n')
    {
      line.trim();

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

  if (millis() - lastSignalReq > 10000UL)
  {
    lastSignalReq = millis();
    GSM.println("AT+CSQ");
  }
  gpsLoop();
}