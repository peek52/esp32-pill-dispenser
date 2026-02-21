#ifndef NETPIE_H
#define NETPIE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// ============================================
// NETPIE MQTT Module
// ============================================

// --- ประกาศ extern client สำหรับ modules อื่น ---
extern PubSubClient mqttClient;
extern bool netpieConnected;

// --- Functions ---
void netpieSetup();
void netpieLoop();
void netpieConnect();
void updateShadow(String status, String detail);
void updateShadowKey(String key, String value);
void updateShadowKey(String key, int value);

// --- Callback type ---
typedef void (*ScheduleCallback)(JsonDocument &doc);
void setScheduleCallback(ScheduleCallback callback);

typedef void (*DispenseCallback)(String source);
void setDispenseCallback(DispenseCallback callback);

typedef void (*NetpieDispenseCallback)(int slot);
void setNetpieDispenseCallback(NetpieDispenseCallback callback);

#endif // NETPIE_H
