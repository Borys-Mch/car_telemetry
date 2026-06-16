#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

extern StaticJsonDocument<256> mqttDoc;

extern const char *TOPIC_AVAIL;
extern const char *TOPIC_STATUS;
extern const char *TOPIC_SIGNAL;
extern const char *TOPIC_BARRIER_CMD;

bool mqttInit();
void mqttSend(const String &topic, const String &payload, bool retain);

template <typename Builder>
void mqttPublishJson(const String &topic, Builder builder, bool retain = false)
{
  mqttDoc.clear();
  JsonObject root = mqttDoc.to<JsonObject>();
  builder(root);

  String payload;
  serializeJson(mqttDoc, payload);
  mqttSend(topic, payload, retain);
}

void mqttHandleIncoming(const String &topic, const String &payload);