#pragma once
#include <Arduino.h>

struct GpsFix
{
  float lat;
  float lon;
  uint8_t sats;
  bool valid;
};

void gpsInit();              // увімкнути GNSS
void gpsLoop();              // викликати з loop(), щоб опитувати модем
bool gpsGetFix(GpsFix &out); // отримати останній валідний фікс