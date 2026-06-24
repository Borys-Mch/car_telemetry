#pragma once
#include "config.h"
#include "secrets.h"

extern TinyGsm modem;
extern TinyGsmClient gsmClient;
extern SSLClient sslClient;
extern PubSubClient mqttClient;

bool initModem();
bool connectToGPRS();