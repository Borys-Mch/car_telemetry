/*
 * Car Telemetry - MQTT + FreeRTOS tasks
 * ESP32-S3 + A7670E + ELM327
 *
 * Бібліотеки: ArduinoJson
 * Arduino IDE → Tools → USB CDC On Boot: Enabled
 */

#include "config.h"
#include "secrets.h"
#include "modem.h"
#include "mqtt.h"
#include "gps.h"

String line;
unsigned long lastSignalReq = 0;

// стан кнопки
static bool lastBtnState = HIGH;
static unsigned long lastDebounce = 0;
static const unsigned long DEBOUNCE_MS = 50;

// стан дзвінка
enum class CallState
{
  Idle,
  Calling
};
static CallState callState = CallState::Idle;
static unsigned long callStartMs = 0;
static const unsigned long CALL_DURATION_MS = 8000;

enum class MqttRxState
{
  Idle,
  ExpectTopic,
  ExpectPayload
};
static MqttRxState mqttRxState = MqttRxState::Idle;
static String mqttRxTopic;

unsigned long lastGpsReq = 0;

void startBarrierCall()
{
  if (callState == CallState::Calling)
  {
    Serial.println("[CALL] Уже йде дзвінок, ігноруємо кнопку");
    return;
  }

  Serial.println("[CALL] Старт дзвінка на шлагбаум...");
  GSM.printf("ATD%s;\r\n", BARRIER_NUMBER);

  callState = CallState::Calling;
  callStartMs = millis();

  mqttSendCallStatus("", "calling");
}

void setup()
{
  Serial.begin(GSM_BAUD);
  delay(300);

  Serial.println();
  Serial.println("=== ESP32 boot ===");

  if (!modemInit())
  {
    Serial.println("MODEM INIT FAILED");
  }
  else
  {
    Serial.println("MODEM INIT OK");
  }

  sendATWait("AT+CGNSSPWR=1", "OK", 3000);

  // MQTT: перше підключення
  mqttConnect();

  if (mqttConnected)
  {
    delay(1500);
    mqttSubscribeCmd();
    delay(1000);
    mqttPublishSignalDiscovery();
    delay(500);
    mqttPublishCallStatusDiscovery();
    delay(500);
    mqttSendIncomingDiscovery();
    delay(500);
    mqttPublishGateButtonDiscovery();
    delay(500);
    mqttPublishHangupButtonDiscovery();
    delay(500);
    mqttSendDiscoveryGps();
    delay(500);
    mqttPublishObdDiscovery();
  }
  else
  {
    Serial.println("[SETUP] MQTT not connected, watchdog will retry");
  }

  pinMode(BTN_PIN, INPUT_PULLUP);
}

void loop()
{
  // ── читаємо GSM UART ──────────────────────────────────────────────────────
  while (GSM.available())
  {
    char c = GSM.read();
    Serial.write(c);

    if (c == '\n')
    {
      line.trim();

      // передаємо кожен рядок URC-обробнику MQTT
      mqttHandleURC(line);

      // ── CSQ ──────────────────────────────────────────────────────────────
      if (line.startsWith("+CSQ:"))
      {
        int comma = line.indexOf(',');
        int rssi = line.substring(6, comma).toInt();
        mqttSendSignal(rssi);
      }

      // ── завершення дзвінка ────────────────────────────────────────────────
      if (line.indexOf("NO CARRIER") != -1)
      {
        Serial.println("[CALL] NO CARRIER -> idle");
        callState = CallState::Idle;
        mqttSendCallStatus("", "idle");
      }

      if (line.indexOf("VOICE CALL: END") != -1)
      {
        callState = CallState::Idle;
        mqttSendCallStatus("", "idle");
      }

      // ── GPS ───────────────────────────────────────────────────────────────
      gpsParse(line);

      // ── вхідний дзвінок ───────────────────────────────────────────────────
      if (line.startsWith("+CLIP:"))
      {
        int firstQuote = line.indexOf('\"');
        int secondQuote = line.indexOf('\"', firstQuote + 1);

        if (firstQuote != -1 && secondQuote != -1 && secondQuote > firstQuote + 1)
        {
          String number = line.substring(firstQuote + 1, secondQuote);
          Serial.println("Incoming: " + number);
          mqttSendCallStatus(number, "ringing");
        }
      }

      // ── MQTT RX: topic ────────────────────────────────────────────────────
      if (line.startsWith("+CMQTTRXTOPIC:"))
      {
        mqttRxState = MqttRxState::ExpectTopic;
        mqttRxTopic = "";
      }
      else if (mqttRxState == MqttRxState::ExpectTopic)
      {
        mqttRxTopic = line;
        mqttRxState = MqttRxState::ExpectPayload;
      }
      else if (mqttRxState == MqttRxState::ExpectPayload)
      {
        if (line.startsWith("+CMQTTRXPAYLOAD:"))
        {
          // пропускаємо заголовок, чекаємо наступний рядок
        }
        else
        {
          String payload = line;
          Serial.println("[MQTT RX] " + mqttRxTopic + " : " + payload);

          if (mqttRxTopic == "car/cmd")
          {
            if (payload == "gate")
            {
              startBarrierCall();
            }
            else if (payload == "hangup")
            {
              Serial.println("[CALL] Hangup from HA");
              GSM.println("AT+CHUP");
              callState = CallState::Idle;
              mqttSendCallStatus("", "idle");
            }
          }

          mqttRxState = MqttRxState::Idle;
        }
      }

      line = "";
    }
    else
    {
      line += c;
    }
  }

  // ── CSQ запит кожні 10 сек ────────────────────────────────────────────────
  if (millis() - lastSignalReq > 10000UL)
  {
    lastSignalReq = millis();
    GSM.println("AT+CSQ");
  }

  // ── GPS запит кожні 10 сек ────────────────────────────────────────────────
  if (millis() - lastGpsReq > 10000UL)
  {
    lastGpsReq = millis();
    GSM.println("AT+CGNSSINFO");
  }

  // ── фізична кнопка ───────────────────────────────────────────────────────
  int raw = digitalRead(BTN_PIN);

  if (raw != lastBtnState)
  {
    lastDebounce = millis();
    lastBtnState = raw;
  }

  if ((millis() - lastDebounce) > DEBOUNCE_MS)
  {
    if (raw == LOW)
    {
      startBarrierCall();
    }
  }

  // ── MQTT watchdog: перевірка і reconnect ──────────────────────────────────
  mqttWatchdog();

}
