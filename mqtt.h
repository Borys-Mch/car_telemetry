#pragma once
#include "config.h"

void mqttConnect();
void mqttSendSignal(int rssi);

// discovery helper
using HaDiscoveryBuilder = void (*)(JsonObject &);

void mqttPublishDiscovery(const String &topic, HaDiscoveryBuilder builder);
void mqttPublishSignalDiscovery();