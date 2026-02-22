#include "wifi_manager.h"
#include "config.h"
#include "scheduler.h"
#include <Network.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp_sntp.h>
#include <time.h>


static WiFiManager wm;
static bool manualConnectPending = false;
static String targetSSID = "";
static String targetPass = "";
static unsigned long manualConnectStartMs = 0;
static bool hasSyncedNTPOnBoot = false;

static volatile bool ntpSynced = false;

void time_sync_notification_cb(struct timeval *tv) {
  Serial.println("\n[WiFi] SNTP Sync Notification Received!");
  ntpSynced = true;
}

void wifiSyncNTP() {
  Serial.print("[WiFi] Syncing NTP Time...");
  ntpSynced = false;

  // Register callback so we know when the actual UDP response arrives,
  // bypassing any fake "success" from the cached DS3231 RTC time.
  sntp_set_time_sync_notification_cb(time_sync_notification_cb);
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

  int retry = 0;
  // Wait up to 15 seconds for the SNTP callback to fire
  while (!ntpSynced && retry < 30) {
    Serial.print(".");
    delay(500);
    retry++;
  }
  Serial.println();

  if (ntpSynced) {
    Serial.println("[WiFi] NTP Sync successful.");
    schedulerSyncNTP(); // Write guaranteed internet time to RTC
  } else {
    Serial.println(
        "[WiFi] NTP Sync failed (timeout). Re-check network firewall/DNS.");
  }
}

void wifiSetup(void) {
  // Set WiFi to station mode and disconnect from an AP if it was previously
  // connected
  WiFi.mode(WIFI_STA);
  // Do not auto-connect yet, let the UI or user trigger it if needed, or rely
  // on WiFiManager Actually, WiFi automatically reconnects if credentials are
  // saved. We will let the system auto connect in the background if it has
  // credentials.
  WiFi.begin();
}

void wifiLoop(void) {
  if (manualConnectPending) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("[WiFi] Manual connection successful!");
      wifiSyncNTP();
      manualConnectPending = false;
    } else if (millis() - manualConnectStartMs > 20000) {
      Serial.println(
          "[WiFi] Manual connection timeout. Invalid password or no signal.");
      WiFi.disconnect();
      manualConnectPending = false;
    }
  } else {
    // Background connection check
    if (wifiIsConnected() && !hasSyncedNTPOnBoot) {
      Serial.println("[WiFi] Detected background connection.");
      wifiSyncNTP();
      hasSyncedNTPOnBoot = true;
    }
  }
}

bool wifiIsConnected(void) { return WiFi.status() == WL_CONNECTED; }

String wifiGetSSID(void) {
  if (wifiIsConnected()) {
    return WiFi.SSID();
  }
  return "Not Connected";
}

String wifiGetIP(void) {
  if (wifiIsConnected()) {
    return WiFi.localIP().toString();
  }
  return "0.0.0.0";
}

void wifiStartPortal(void) {
  Serial.println("[WiFi] Starting Captive Portal (Med-Dispenser)");

  // Custom styling (optional)
  wm.setCustomHeadElement(
      "<style>body{background-color:#2c3e50;color:white;} "
      ".c{text-align:center;} "
      "button{background-color:#27ae60;border:none;color:white;padding:15px "
      "32px;text-align:center;text-decoration:none;display:inline-block;font-"
      "size:16px;margin:4px 2px;cursor:pointer;border-radius:8px;} </style>");

  // Set configuration portal timeout to 3 minutes so it doesn't block forever
  wm.setConfigPortalTimeout(180);

  if (!wm.startConfigPortal("Med-Dispenser")) {
    Serial.println(
        "[WiFi] Failed to connect and hit configuration portal timeout");
    delay(3000);
    // Could just return instead of restarting, but WiFiManager docs often
    // suggest restart. ESP.restart();
  }

  // If we get here, we are connected (or timeout hit)
  if (wifiIsConnected()) {
    Serial.printf("[WiFi] Connected! IP: %s\n", wifiGetIP().c_str());
    wifiSyncNTP();
  }
}

void wifiConnectManual(const char *ssid, const char *pass) {
  Serial.printf("[WiFi] Manual connect to: %s\n", ssid);
  targetSSID = ssid;
  targetPass = pass;
  manualConnectPending = true;
  manualConnectStartMs = millis();

  // Minimal commands to prevent esp-hosted re-init crash on ESP32-P4
  WiFi.begin(ssid, pass);
}

void wifiForget(void) {
  Serial.println("[WiFi] Forgetting credentials...");
  wm.resetSettings();
  WiFi.disconnect();
  delay(100);
  Serial.println("[WiFi] Forgotten.");
}
