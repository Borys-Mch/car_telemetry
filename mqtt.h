#pragma once
#include "config.h"
#include "secrets.h"

// Глобальні об'єкти
extern TinyGsm modem;
extern TinyGsmClient gsmClient;
extern SSLClient sslClient;
extern PubSubClient mqttClient;

// Функції
bool initMQTT();
void connectMQTT();