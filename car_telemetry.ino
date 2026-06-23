#define TINY_GSM_MODEM_SIM7600
#include "secrets.h"
#include "config.h"

// ==============================================
// 4. ІНШІ НАЛАШТУВАННЯ
// ==============================================

const char clientId[] = "test_client";

// ==============================================
// 5. ІНІЦІАЛІЗАЦІЯ
// ==============================================
TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
SSLClient sslClient(&client);
PubSubClient mqttClient(sslClient);

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

  // Вимкнути перевірку сертифікатів (insecure mode)
  sslClient.setInsecure();

  // Налаштування MQTT
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(callback);

  connectMQTT();
}

void connectMQTT()
{
  Serial.print("Підключення до MQTT...");
  while (!mqttClient.connected())
  {
    if (mqttClient.connect(clientId, MQTT_USER, MQTT_PASS))
    {
      Serial.println(" підключено!");
      mqttClient.subscribe("test/topic");
    }
    else
    {
      Serial.print(" помилка, код: ");
      Serial.print(mqttClient.state());
      Serial.println(" спроба через 5 секунд...");
      delay(5000);
    }
  }
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Повідомлення: ");
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();
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