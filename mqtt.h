#pragma once
#include "config.h"
#include "secrets.h"
#include "modem.h"

bool initMQTT();
bool connectToMQTT();
void mqttLoop();
void publishDiscovery();
void publishSignalSensor();
void publishIncomingCallSensor();
void publishCallStatusSensor();
void publishRebootButton();
void publishBarrierButton();