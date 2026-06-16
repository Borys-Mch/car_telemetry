#include "modem.h"
#include "mqtt_at.h"
#include "gps_at.h"
#include "secrets.h"

String line;
unsigned long lastSignalReq = 0;

enum class CallState
{
  Idle,
  Calling
};
static CallState callState = CallState::Idle;
static unsigned long callStartMs = 0;
static const unsigned long CALL_DURATION_MS = 8000;

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

void publishCallStatus(const char *state)
{
  mqttPublishJson(TOPIC_STATUS, [state](JsonObject &root)
                  {
                    root["state"] = state; // "idle" / "calling" / "error"
                  },
                  false);
}

void barrierCallStart()
{
  if (callState == CallState::Calling)
  {
    Serial.println("[CALL] Уже йде дзвінок, ігноруємо команду");
    return;
  }

  Serial.println("[CALL] Старт дзвінка на шлагбаум...");

  publishCallStatus("calling");

  GSM.printf("ATD%s;\r\n", BARRIER_NUM);
  callState = CallState::Calling;
  callStartMs = millis();
}

void barrierCallLoop()
{
  if (callState == CallState::Calling &&
      millis() - callStartMs > CALL_DURATION_MS)
  {

    Serial.println("[CALL] Завершення дзвінка");
    GSM.println("AT+CHUP");
    callState = CallState::Idle;
    publishCallStatus("idle");
  }
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
  static bool testCallDone = false;

  if (!testCallDone && millis() > 15000)
  { // через 15 сек після старту
    testCallDone = true;
    barrierCallStart();
  }
  barrierCallLoop();
  gpsLoop();
}