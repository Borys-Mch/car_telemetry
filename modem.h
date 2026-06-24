#pragma once
#include "config.h"
#include "secrets.h"
#include "mqtt.h"

extern TinyGsm modem;
extern TinyGsmClient gsmClient;
extern SSLClient sslClient;
extern PubSubClient mqttClient;

bool initModem();
bool connectToGPRS();
int getSignalQuality();
String getIncomingNumber();
bool callBarrier();
void setCallStatus(const String &status);
String getCallStatus();