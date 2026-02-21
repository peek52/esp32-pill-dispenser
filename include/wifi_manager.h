#pragma once
#include <Arduino.h>

// ============================================================
// Public API for WiFi Management
// ============================================================
void wifiSetup(void);
void wifiLoop(void);

// Connection Status
bool wifiIsConnected(void);
String wifiGetSSID(void);
String wifiGetIP(void);

// Connection Methods
void wifiStartPortal(void); // Starts WiFiManager captive portal (blocking)
void wifiConnectManual(const char *ssid,
                       const char *pass); // Manual OSK connection
void wifiForget(void); // Forgets stored credentials and disconnects
