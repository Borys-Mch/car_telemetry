#include "secrets.h"
#include "config.h"
#include "mqtt.h"
#include "modem.h"

void setup()
{
  Serial.begin(115200);

  initModem();
  connectToGPRS();

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