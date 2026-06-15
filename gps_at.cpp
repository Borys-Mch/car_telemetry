#include "gps_at.h"
#include "modem.h"

// внутрішній стан
static GpsFix lastFix = {0, 0, 0, false};
static unsigned long lastQuery = 0;
static const unsigned long GPS_INTERVAL_MS = 10000; // 10 сек

void gpsInit()
{
  // Вмикаємо живлення GNSS [web:34][web:98]
  sendATWait("AT+CGNSSPWR=1", "OK", 3000);
  // Можна дати йому кілька секунд, але це радше для першого фіксу
  delay(1000);
  Serial.println("[GPS] GNSS power ON");
}

// Очікуємо +CGNSSINFO: ...
static void parseCgnssInfo(const String &line)
{
  // Очікуємо щось типу:
  // +CGNSSINFO: 1,1,20260215121530.000,50.450123,30.523456,-13.4,0.0,0.0,2,,,...,8,15,,... [web:95][web:101]
  // Нас цікавлять: latitude, longitude, used satellites

  int firstColon = line.indexOf(':');
  if (firstColon == -1)
    return;

  String data = line.substring(firstColon + 1);
  data.trim();

  // Розбиваємо по комах
  int fieldIndex = 0;
  int start = 0;
  float lat = 0, lon = 0;
  int sats = 0;

  for (int i = 0; i <= data.length(); i++)
  {
    if (i == data.length() || data[i] == ',')
    {
      String token = data.substring(start, i);
      token.trim();

      // Поля приблизно:
      // 0: <mode>
      // 1: <gps_svs>
      // 2: <utc>
      // 3: lat
      // 4: lon
      // ...
      // далі ближче до кінця є satellites, але різні прошивки можуть відрізнятись [web:95][web:96]

      if (fieldIndex == 3 && token.length() > 0)
      {
        lat = token.toFloat();
      }
      if (fieldIndex == 4 && token.length() > 0)
      {
        lon = token.toFloat();
      }
      // як мінімум візьмемо gps_svs як "кількість супутників GPS" [web:101]
      if (fieldIndex == 1 && token.length() > 0)
      {
        sats = token.toInt();
      }

      fieldIndex++;
      start = i + 1;
    }
  }

  if (lat != 0.0f && lon != 0.0f)
  {
    lastFix.lat = lat;
    lastFix.lon = lon;
    lastFix.sats = (uint8_t)sats;
    lastFix.valid = true;

    Serial.print("[GPS] Fix: lat=");
    Serial.print(lat, 6);
    Serial.print(" lon=");
    Serial.print(lon, 6);
    Serial.print(" sats=");
    Serial.println(sats);
  }
  else
  {
    Serial.println("[GPS] No valid fix yet");
    lastFix.valid = false;
  }
}

void gpsLoop()
{
  // 1) періодично запитуємо AT+CGNSSINFO
  if (millis() - lastQuery > GPS_INTERVAL_MS)
  {
    lastQuery = millis();
    GSM.println("AT+CGNSSINFO");
  }

  // 2) парсимо відповіді — інтегруємось у загальний потік GSM
  //   ЗАМІТКА: якщо у тебе вже є глобальний reader у car_telemetry.ino,
  //   можна замість дублювання викликувати з нього parseCgnssInfo(line).
  while (GSM.available())
  {
    char c = GSM.read();
    Serial.write(c); // поки що просто дзеркалимо в Serial

    static String line;
    if (c == '\n')
    {
      line.trim();
      if (line.startsWith("+CGNSSINFO:"))
      {
        parseCgnssInfo(line);
      }
      line = "";
    }
    else
    {
      line += c;
    }
  }
}

bool gpsGetFix(GpsFix &out)
{
  out = lastFix;
  return out.valid;
}