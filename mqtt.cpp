#include "mqtt.h"

TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
SSLClient sslClient(&client);
PubSubClient mqttClient(sslClient);

bool initMQTT()
{
  Serial.println("Ініціалізація MQTT...");

  // Вимкнути перевірку сертифікатів (insecure mode)
  sslClient.setInsecure();

  // Налаштування MQTT
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);

  return true;
}

void connectMQTT()
{
  Serial.print("Підключення до MQTT...");
  while (!mqttClient.connected())
  {
    if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS))
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