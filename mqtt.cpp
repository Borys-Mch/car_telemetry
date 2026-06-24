#include "mqtt.h"

void callback(char *topic, byte *payload, unsigned int length)
{
  String message = String((char *)payload).substring(0, length);
  if (message == "reboot")
  {
    Serial.println("Перезавантаження...");
    ESP.restart();
  }

  if (message == "gate")
  {
    Serial.println("Відкриття шлагбаума...");
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

bool initMQTT()
{
  mqttClient.setBufferSize(512);
  Serial.println("Ініціалізація MQTT...");
  sslClient.setInsecure();
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(callback);
  return true;
}

bool connectToMQTT()
{
  Serial.print("Підключення до MQTT...");

  if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS))
  {
    Serial.println("ПІДКЛЮЧЕНО!");
    mqttClient.subscribe(MQTT_TOPIC_SUBSCRIBE);

    publishDiscovery();
    return true;
  }
  else
  {
    Serial.print("ПОМИЛКА! Код: ");
    Serial.println(mqttClient.state());
    return false;
  }
}

void mqttLoop()
{
  if (!mqttClient.connected())
  {
    Serial.println("MQTT відключено. Спроба перепідключення...");
    connectToMQTT();
  }
  mqttClient.loop();
}

void publishDiscovery()
{
  publishSignalSensor();
  publishIncomingCallSensor();
  publishCallStatusSensor();
  publishRebootButton();
  publishBarrierButton();
}

void publishSignalSensor()
{
  String topic = String(MQTT_DISCOVERY_PREFIX) + "/sensor/" + MQTT_CLIENT_ID + "/signal/config";

  String payload = "{";
  payload += "\"name\":\"Modem Signal\",";
  payload += "\"state_topic\":\"car/telemetry\",";
  payload += "\"value_template\":\"{{ value_json.signal }}\",";
  payload += "\"unit_of_measurement\":\"dBm\",";
  payload += "\"icon\":\"mdi:signal\",";
  payload += "\"device_class\":\"signal_strength\",";
  payload += "\"unique_id\":\"car_tracker_signal\",";
  payload += "\"device\":{";
  payload += "\"identifiers\":[\"" + String(MQTT_CLIENT_ID) + "\"],";
  payload += "\"name\":\"" + String(MQTT_CLIENT_NAME) + "\",";
  payload += "\"manufacturer\":\"" + String(MQTT_CLIENT_MANUFACTURER) + "\",";
  payload += "\"model\":\"" + String(MQTT_CLIENT_MODEL) + "\"";
  payload += "}}";

  mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

void publishIncomingCallSensor()
{
  String topic = String(MQTT_DISCOVERY_PREFIX) + "/sensor/" + MQTT_CLIENT_ID + "/incoming_call/config";

  String payload = "{";
  payload += "\"name\":\"Incoming Call\",";
  payload += "\"state_topic\":\"car/telemetry\",";
  payload += "\"value_template\":\"{{ value_json.incoming_call }}\",";
  payload += "\"icon\":\"mdi:phone-ring\",";
  payload += "\"unique_id\":\"car_incoming_call\",";
  payload += "\"device\":{";
  payload += "\"identifiers\":[\"" + String(MQTT_CLIENT_ID) + "\"]";
  payload += "}}";

  mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

void publishCallStatusSensor()
{
  String topic = String(MQTT_DISCOVERY_PREFIX) + "/sensor/" + MQTT_CLIENT_ID + "/call_status/config";

  String payload = "{";
  payload += "\"name\":\"Call Status\",";
  payload += "\"state_topic\":\"car/telemetry\",";
  payload += "\"value_template\":\"{{ value_json.call_status }}\",";
  payload += "\"icon\":\"mdi:phone\",";
  payload += "\"unique_id\":\"car_call_status\",";
  payload += "\"device\":{";
  payload += "\"identifiers\":[\"" + String(MQTT_CLIENT_ID) + "\"]";
  payload += "}}";

  mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

void publishRebootButton()
{
  String topic = String(MQTT_DISCOVERY_PREFIX) + "/button/" + MQTT_CLIENT_ID + "/reboot/config";

  String payload = "{";
  payload += "\"name\":\"Reboot ESP32\",";
  payload += "\"command_topic\":\"car/cmd\",";
  payload += "\"payload_press\":\"reboot\",";
  payload += "\"icon\":\"mdi:restart\",";
  payload += "\"unique_id\":\"car_esp_reboot\",";
  payload += "\"device\":{";
  payload += "\"identifiers\":[\"" + String(MQTT_CLIENT_ID) + "\"]";
  payload += "}}";

  mqttClient.publish(topic.c_str(), payload.c_str(), true);
}

void publishBarrierButton()
{
  String topic = String(MQTT_DISCOVERY_PREFIX) + "/button/" + MQTT_CLIENT_ID + "/gate/config";

  String payload = "{";
  payload += "\"name\":\"Gate Open\",";
  payload += "\"command_topic\":\"car/cmd\",";
  payload += "\"payload_press\":\"gate\",";
  payload += "\"icon\":\"mdi:boom-gate-outline\",";
  payload += "\"unique_id\":\"car_gate_btn\",";
  payload += "\"device\":{";
  payload += "\"identifiers\":[\"" + String(MQTT_CLIENT_ID) + "\"]";
  payload += "}}";

  mqttClient.publish(topic.c_str(), payload.c_str(), true);
}