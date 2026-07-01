#include "gps.h"

bool isValidUkraineRange(float lat, float lon)
{
  if (lat < 44.0f || lat > 53.0f)
    return false;
  if (lon < 22.0f || lon > 42.0f)
    return false;
  return true;
}

void gpsParse(const String &line)
{
  if (line.startsWith("+CGNSSINFO:"))
  {
    if (line.indexOf(",,,,") != -1)
    {
      Serial.println("[GPS] No fix yet");
      mqttSendGps(0, 0, 0, "GPS unavailable", false);
    }
    else
    {
      int idx[20];
      int count = 0;
      for (int i = 0; i < (int)line.length() && count < 20; i++)
      {
        if (line[i] == ',')
          idx[count++] = i;
      }

      if (count >= 8)
      {
        String latStr = line.substring(idx[4] + 1, idx[5]);
        String latDir = line.substring(idx[5] + 1, idx[6]);
        String lonStr = line.substring(idx[6] + 1, idx[7]);
        String lonDir = line.substring(idx[7] + 1, idx[8]);

        latStr.trim();
        lonStr.trim();
        latDir.trim();
        lonDir.trim();

        float latDeg = latStr.toFloat();
        float lonDeg = lonStr.toFloat();

        if (latDir == "S")
          latDeg = -latDeg;
        if (lonDir == "W")
          lonDeg = -lonDeg;

        if (isValidUkraineRange(latDeg, lonDeg))
        {
          Serial.print("[GPS] Fix: ");
          Serial.print(latDeg, 6);
          Serial.print(", ");
          Serial.println(lonDeg, 6);

          int sats = line.substring(idx[0] + 1, idx[1]).toInt();
          mqttSendGps(latDeg, lonDeg, sats, "", true);
        }
        else
        {
          Serial.println("[GPS] Fix out of UA range, ignored");
          mqttSendGps(0, 0, 0, "Electronic warfare", false);
        }
      }
    }
  }
}