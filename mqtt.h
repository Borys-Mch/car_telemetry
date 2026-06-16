#pragma once
#include <Arduino.h>

void mqttConnect();
void mqttTestPublish();
void mqttSendSignal(int rssi);