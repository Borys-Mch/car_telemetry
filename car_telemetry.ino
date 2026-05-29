/*
 * Car Telemetry - MQTT + FreeRTOS tasks
 * ESP32-S3 + A7670E (UART1 GPIO18/17) + ELM327 (UART0 GPIO44/43)
 *
 * Бібліотеки: ArduinoJson
 * Arduino IDE → Tools → USB CDC On Boot: Enabled
 */

#include <ArduinoJson.h>
#include "esp_task_wdt.h"
#include "secrets.h"

// ===== ПІНИ =================================================
#define GSM_RX 18
#define GSM_TX 17
#define GSM_BAUD 115200
#define PWK_PIN 4
#define BTN_PIN 5

// ===== UART =================================================
HardwareSerial GSM(2);

void setup()
{
  Serial.begin(115200);

  delay(1000);
  Serial.println("Serial connection ready");

  GSM.begin(GSM_BAUD, SERIAL_8N1, GSM_RX, GSM_TX);
  GSM.println("AT");
}

void loop()
{
  while (GSM.available())
  {
    String resp = GSM.readStringUntil('\n');
    resp.trim();
    if (resp.length() > 0)
    {
      Serial.println("Modem: " + resp);
    }
  }

  static unsigned long lastRun = millis() - 18000;
  if (millis() - lastRun > 20000)
  { // Оновлювати кожні 20 секунд через 2 секунди.
    lastRun = millis();

    GSM.println("AT+CSQ");      // Signalstärke
    GSM.println("AT+CEREG?");   // LTE Netzregistrierung
    GSM.println("AT+CGATT?");   // Datenregistrierung
    GSM.println("AT+CGACT?");   // PDP Kontext aktiv? (Datenverbindung)
    GSM.println("AT+CGPADDR");  // IP-Adresse
    GSM.println("AT+NETOPEN?"); // Socket-Service Status
  }
}