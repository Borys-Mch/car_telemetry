#pragma once
#include "config.h"

// Простий інтерфейс ініціалізації
bool modemInit();

// Якщо захочеш, ці утиліти теж винесемо в хедер:
bool waitForAT(uint32_t totalTimeout = 30000);
bool waitForSIM(uint32_t totalTimeout = 20000);
bool waitForNetwork(uint32_t totalTimeout = 45000);
bool sendATWait(const char *cmd, const char *ok = "OK", uint32_t timeout = 2000);
void sendAT(const char *cmd);
void flushGSM();