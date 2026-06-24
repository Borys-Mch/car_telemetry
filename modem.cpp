#include "modem.h"

TinyGsm modem(SerialAT);
TinyGsmClient gsmClient(modem);
SSLClient sslClient(&gsmClient);
PubSubClient mqttClient(sslClient);

bool initModem()
{
  // Ініціалізація UART для модема
  SerialAT.begin(115200, SERIAL_8N1, GSM_RX, GSM_TX);
  delay(3000);

  Serial.println("Запуск...");

  // Рестарт модема
  modem.restart();

  String modemInfo = modem.getModemInfo();
  if (modemInfo.length() > 0)
  {
    Serial.println("Модем: " + modemInfo);
    return true;
  }
  return false;
}

bool connectToGPRS()
{
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
  return true;
}