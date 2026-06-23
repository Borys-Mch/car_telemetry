#include "secrets.h"
#include "config.h"
#include "mqtt.h"

// ==============================================
// 5. ІНІЦІАЛІЗАЦІЯ
// ==============================================

void setup()
{
  Serial.begin(115200);

  // Ініціалізація UART для модема
  SerialAT.begin(115200, SERIAL_8N1, GSM_RX, GSM_TX);
  delay(3000);

  Serial.println("Запуск...");

  // Рестарт модема
  modem.restart();

  String modemInfo = modem.getModemInfo();
  Serial.println("Модем: " + modemInfo);

  // Підключення до GPRS
  Serial.print("Підключення до мережі ");
  Serial.print(APN);
  if (!modem.gprsConnect(APN))
  {
    Serial.println(" Помилка!");
    while (true)
      delay(1000);
  }
  Serial.println(" OK");
  Serial.print("IP: ");
  Serial.println(modem.getLocalIP());

  initMQTT();
  connectMQTT();
}

void loop()
{
  if (!mqttClient.connected())
  {
    connectMQTT();
  }
  mqttClient.loop();

  static long lastMsg = 0;
  if (millis() - lastMsg > 10000)
  {
    lastMsg = millis();
    if (mqttClient.publish("test/topic", "Hello from A7670E!"))
    {
      Serial.println("Повідомлення відправлено");
    }
  }
}