#include "offline_module.h"
#include "config.h"
#include "logging.h"
#include "netpie.h"
#include "scheduler.h" // For rtc
#include "wifi_manager.h"
#include <FS.h>
#include <LittleFS.h>

#define OFFLINE_LOG_FILE "/offline_logs.txt"

void offlineSetup() {
  if (!LittleFS.begin(true)) {
    LOG_ERROR("[Offline] LittleFS Mount Failed");
    return;
  }
  LOG_INFO("[Offline] LittleFS Mounted");
}

void addToOfflineQueue(String type, String data) {
  File file = LittleFS.open(OFFLINE_LOG_FILE, FILE_READ);
  if (file) {
    size_t fsize = file.size();
    file.close();
    if (fsize > 50000) { // Limit to 50KB
      LOG_WARN("[Offline] Offline log exceeded 50KB. Removing old logs.");
      LittleFS.remove(OFFLINE_LOG_FILE);
    }
  }

  file = LittleFS.open(OFFLINE_LOG_FILE, FILE_APPEND);
  if (!file) {
    LOG_ERROR("[Offline] Failed to open log file for appending");
    return;
  }

  // Format: timestamp,type,data
  unsigned long ts = schedulerGetUnixTime();
  file.printf("%lu,%s,%s\n", ts, type.c_str(), data.c_str());
  file.close();

  LOG_INFO("[Offline] Event logged to file: %s", type.c_str());
}

void syncOfflineQueue() {
  if (!wifiIsConnected())
    return;

  if (!LittleFS.exists(OFFLINE_LOG_FILE))
    return;

  File file = LittleFS.open(OFFLINE_LOG_FILE, FILE_READ);
  if (!file)
    return;

  LOG_INFO("[Offline] Syncing offline logs...");

  int count = 0;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0)
      continue;

    // Format: timestamp,type,data
    // Publish each event to shadow as a status update
    int firstComma = line.indexOf(',');
    int secondComma = line.indexOf(',', firstComma + 1);
    if (firstComma > 0 && secondComma > firstComma) {
      String type = line.substring(firstComma + 1, secondComma);
      String data = line.substring(secondComma + 1);
      updateShadow(type, "[Offline] " + data);
    }
    count++;
  }
  file.close();

  if (count > 0) {
    LOG_INFO("[Offline] Synced %d offline events", count);
    clearOfflineLogs();
  }
}

String getOfflineLogs() {
  if (!LittleFS.exists(OFFLINE_LOG_FILE))
    return "No offline logs.";

  File file = LittleFS.open(OFFLINE_LOG_FILE, FILE_READ);
  if (!file)
    return "Failed to open logs.";

  String logs = "Offline Activity:\n";
  while (file.available()) {
    logs += file.readStringUntil('\n') + "\n";
  }
  file.close();
  return logs;
}

void clearOfflineLogs() {
  LittleFS.remove(OFFLINE_LOG_FILE);
  LOG_INFO("[Offline] Logs cleared.");
}

void updateShadowOfflineAware(String status, String detail) {
  if (!wifiIsConnected()) {
    addToOfflineQueue(status, detail);
    return;
  }
  updateShadow(status, detail);
}
