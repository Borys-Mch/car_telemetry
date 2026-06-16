/*
 * Car Telemetry - MQTT + FreeRTOS tasks
 * ESP32-S3 + A7670E (UART1 GPIO18/17) + ELM327 (UART0 GPIO44/43)
 *
 * Бібліотеки: ArduinoJson
 * Arduino IDE → Tools → USB CDC On Boot: Enabled
 */

#include "config.h"
#include "secrets.h"
#include "modem.h"
#include "mqtt.h"

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

void startBarrierCall()
{
  if (callState == CallState::Calling)
  {
    Serial.println("[CALL] Уже йде дзвінок, ігноруємо кнопку");
    return;
  }

  Serial.println("[CALL] Старт дзвінка на шлагбаум...");
  GSM.printf("ATD%s;\r\n", BARRIER_NUMBER); // номер з secrets.h

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
  }
}

void setup()
{
  Serial.begin(GSM_BAUD);
  delay(300);

  Serial.println();
  Serial.println("=== ESP32 boot ===");

  // Ініціалізація модема (AT, SIM, мережа)
  if (!modemInit())
  {
    Serial.println("MODEM INIT FAILED");
    // тут можна або зависнути, або перезапускати ESP, але поки що — просто повідомлення
  }
  else
  {
    Serial.println("MODEM INIT OK");
  }

  // MQTT
  mqttConnect();
  delay(2000);
  mqttTestPublish();
  pinMode(BTN_PIN, INPUT_PULLUP); // кнопка на землю
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
        mqttSendSignal(rssi);
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

  // --- обробка фізичної кнопки ---
  int raw = digitalRead(BTN_PIN);

  if (raw != lastBtnState)
  {
    lastDebounce = millis();
    lastBtnState = raw;
  }

  if ((millis() - lastDebounce) > DEBOUNCE_MS)
  {
    // активний фронт: перехід з HIGH в LOW (кнопка натиснута)
    if (raw == LOW)
    {
      startBarrierCall();
    }
  }

  barrierCallLoop();
}