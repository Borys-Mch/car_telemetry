#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "secrets.h"

// ===== ПІНИ =================================================
#define GSM_RX   18
#define GSM_TX   17
#define GSM_BAUD 115200
#define PWK_PIN  4
#define BTN_PIN  5

// ===== UART =================================================
extern HardwareSerial GSM;

// ===== ГЛОБАЛЬНИЙ СТАН ======================================
extern bool modemReady;
