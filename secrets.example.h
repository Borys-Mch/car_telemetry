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
static constexpr const char *MQTT_CLIENT = "car-info";
static constexpr const char *MQTT_CMD_TOPIC = "car/cmd"; // HA публікує сюди "open_gate"

// Номер шлагбауму
static constexpr const char *BARRIER_NUM = "+380690000000";