#pragma once

// APN оператора
static constexpr const char *APN = "internet"; // Lifecell/Vodafone
// static constexpr const char* APN = "www.kyivstar.net";  // Kyivstar

// MQTT брокер (Mosquitto в HA)
static constexpr const char *MQTT_HOST = "site.my";
static constexpr int MQTT_PORT = 8883;
static constexpr const char *MQTT_USER = "user";
static constexpr const char *MQTT_PASS = "paSSword";
static constexpr bool MQTT_USE_TLS = true;
static constexpr const char *MQTT_DISCOVERY_PREFIX = "homeassistant";
static constexpr const char *MQTT_CLIENT_NAME = "Car Info";
static constexpr const char *MQTT_CLIENT_ID = "car-info";
static constexpr const char *MQTT_CLIENT_MANUFACTURER = "DEREN";
static constexpr const char *MQTT_CLIENT_MODEL = "ESP32-S3 + A7670E + ELM327";
static constexpr const char *MQTT_TOPIC_SUBSCRIBE = "car/cmd";
static constexpr const char *MQTT_TOPIC_PUBLISH = "car/telemetry";

// Номер шлагбауму
static constexpr const char *BARRIER_NUMBER = "+380690000000";