#pragma once
#include "config.h"

using HaDiscoveryBuilder = std::function<void(JsonObject &)>;

// ─── стан ────────────────────────────────────────────────────────────────────
extern bool mqttConnected; // true = з'єднання активне

// ─── lifecycle ───────────────────────────────────────────────────────────────
void mqttConnect();   // перше підключення (з setup)
void mqttReconnect(); // повний reconnect з re-subscribe
void mqttWatchdog();  // викликати з loop() — перевіряє і reconnect-ить

// ─── URC-обробник ────────────────────────────────────────────────────────────
// Передавати кожен рядок від GSM: mqttHandleURC(line);
void mqttHandleURC(const String &line);

// ─── publish ─────────────────────────────────────────────────────────────────
void mqttSendSignal(int rssi);
void mqttSendCallStatus(const String &status);
void mqttSendIncomingCall(const String &number);
void mqttClearIncomingCall();
void mqttSendGps(float lat, float lon, int sats);

// ─── subscribe ───────────────────────────────────────────────────────────────
void mqttSubscribeCmd();

// ─── HA discovery ────────────────────────────────────────────────────────────
void mqttPublishDiscovery(const String &topic, HaDiscoveryBuilder builder);
void mqttPublishSignalDiscovery();
void mqttPublishCallStatusDiscovery();
void mqttSendIncomingDiscovery();
void mqttPublishGateButtonDiscovery();
void mqttPublishHangupButtonDiscovery();
void mqttSendDiscoveryGps();