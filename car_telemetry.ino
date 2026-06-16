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
}

void loop()
{
  // поки нічого — тільки перевіряємо, що модем стабільно стартує
}