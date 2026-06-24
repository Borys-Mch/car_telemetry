#include "secrets.h"
#include "config.h"
#include "mqtt.h"
#include "modem.h"

bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;

void setup()
{
  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = 60000,
      .idle_core_mask = 0,
      .trigger_panic = false};
  esp_task_wdt_reconfigure(&wdt_config);

  Serial.begin(115200);

  pinMode(BTN_PIN, INPUT_PULLUP);

  // Ініціалізація модему з перевіркою
  if (!initModem())
  {
    Serial.println("Помилка: модем не відповідає! Перезавантаження...");
    delay(5000);
    ESP.restart();
  }

  // Підключення до GPRS з перевіркою
  if (!connectToGPRS())
  {
    Serial.println("Помилка: GPRS не підключено! Перезавантаження...");
    delay(5000);
    ESP.restart();
  }

  initMQTT();
  connectToMQTT();
}

void loop()
{
  mqttLoop();

  // ОБРОБКА ФІЗИЧНОЇ КНОПКИ
  bool reading = digitalRead(BTN_PIN);

  if (reading != lastButtonState)
  {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > BTN_DEBOUNCE_MS)
  {
    if (reading == LOW && lastButtonState == HIGH)
    {
      Serial.println("Фізична кнопка натиснута!");
      if (getCallStatus() != "Calling")
      {
        callBarrier();
      }
      else
      {
        Serial.println("Дзвінок вже виконується!");
      }
    }
  }
  lastButtonState = reading;

  // ПУБЛІКАЦІЯ ДАНИХ
  static long lastMsg = 0;
  if (millis() - lastMsg > 10000)
  {
    lastMsg = millis();

    int rssi = getSignalQuality();
    String incoming = getIncomingNumber();
    String status = getCallStatus();

    String payload = "{";
    payload += "\"signal\":" + String(rssi) + ",";
    payload += "\"incoming_call\":\"" + incoming + "\",";
    payload += "\"call_status\":\"" + status + "\"";
    payload += "}";

    if (mqttClient.publish("car/telemetry", payload.c_str()))
    {
      Serial.println("Повідомлення відправлено: " + payload);
    }
  }

  yield();
}