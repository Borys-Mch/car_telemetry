#pragma once
#include <Arduino.h>

extern HardwareSerial GSM;

bool modemInit();

bool waitForResponse(const char *ok,
                     const char *err = "ERROR",
                     uint32_t timeout = 2000);
bool sendATWait(const char *cmd,
                const char *ok = "OK",
                uint32_t timeout = 2000);

bool waitForAT(uint32_t totalTimeout = 30000);
bool waitForSIM(uint32_t totalTimeout = 20000);
bool waitForNetwork(uint32_t totalTimeout = 45000);