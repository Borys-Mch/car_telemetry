#pragma once

#define TINY_GSM_MODEM_SIM7600
// ==============================================
// 2. БІБЛІОТЕКИ
// ==============================================
#include <TinyGsmClient.h>
#include <SSLClient.h>
#include <PubSubClient.h>

// ==============================================
// 3. НАЛАШТУВАННЯ UART ДЛЯ ESP32
// ==============================================
#define SerialAT Serial1
#define GSM_RX 18
#define GSM_TX 17
#define GSM_BAUD 115200
#define PWK_PIN 4
#define BTN_PIN 5