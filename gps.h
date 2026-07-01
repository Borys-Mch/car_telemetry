#pragma once
#include "config.h"
#include "mqtt.h"

// перевірка, що координати всередині діапазону України
bool isValidUkraineRange(float lat, float lon);
void gpsParse(const String &line)