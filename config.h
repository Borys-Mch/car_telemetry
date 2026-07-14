#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include "secrets.h"

// ===== ПІНИ =================================================
#define GSM_RX 44
#define GSM_TX 43
#define GSM_BAUD 115200
#define ELM_RX 18
#define ELM_TX 17
#define ELM_BAUD 38400
#define PWK_PIN 4
#define BTN_PIN 5

// ===== UART =================================================
extern HardwareSerial GSM;
extern HardwareSerial ELM;

// ===== ГЛОБАЛЬНИЙ СТАН ======================================
extern bool modemReady;
