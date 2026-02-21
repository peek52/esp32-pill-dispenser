#ifndef OFFLINE_MODULE_H
#define OFFLINE_MODULE_H

#include <Arduino.h>

void offlineSetup();
void addToOfflineQueue(String type, String data);
void syncOfflineQueue();
void updateShadowOfflineAware(String status, String detail);
String getOfflineLogs();
void clearOfflineLogs();

#endif
