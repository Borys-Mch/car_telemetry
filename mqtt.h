#pragma once
#include "config.h"

// тип колбеку для побудови discovery JSON
using HaDiscoveryBuilder = void (*)(JsonObject &);

// хелпер для discovery
void mqttPublishDiscovery(const String &topic, HaDiscoveryBuilder builder);

// MQTT API
void mqttConnect();
void mqttSendSignal(int rssi);
void mqttSendCallStatus(const String &status);

void mqttPublishSignalDiscovery();
void mqttPublishCallStatusDiscovery();
void mqttSendIncomingDiscovery();
void mqttSendIncomingCall(const String &number);
void mqttClearIncomingCall();
void mqttPublishGateButtonDiscovery();
void mqttPublishHangupButtonDiscovery();
void mqttSendGps(float lat, float lon, int sats);
void mqttSendDiscoveryGps();
void mqttSubscribeCmd();