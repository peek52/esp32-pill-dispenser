#include "netpie.h"
#include "config.h"
#include "logging.h"
#include "secrets.h"
#include <ArduinoJson.h>
#include <WiFi.h>

// ============================================
// NETPIE MQTT Implementation
// ============================================

WiFiClient espClient;
PubSubClient mqttClient(espClient);
bool netpieConnected = false;

// Callbacks
static ScheduleCallback onScheduleUpdate = nullptr;
static DispenseCallback onDispenseCommand = nullptr;
static NetpieDispenseCallback onNetpieDispense = nullptr;

// --- Set Callbacks ---
void setScheduleCallback(ScheduleCallback callback) {
  onScheduleUpdate = callback;
}

void setDispenseCallback(DispenseCallback callback) {
  onDispenseCommand = callback;
}

void setNetpieDispenseCallback(NetpieDispenseCallback callback) {
  onNetpieDispense = callback;
}

// --- MQTT Callback ---
void mqttCallback(char *topic, byte *payload, unsigned int length) {
  LOG_INFO("[NETPIE] === Message arrived ===");
  LOG_INFO("[NETPIE] Topic: %s", topic);

  // Buffer protection
  static char msg[2049];
  unsigned int safeLen = (length < 2048) ? length : 2048;
  memcpy(msg, payload, safeLen);
  msg[safeLen] = '\0';

  LOG_INFO("[NETPIE] Payload: %s", msg);

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, msg);

  if (error) {
    LOG_ERROR("[NETPIE] JSON parse failed: %s", error.c_str());
    return;
  }

  // Handle Command (e.g. from Dashboard Button)
  if (doc.containsKey("action")) {
    String action = doc["action"].as<String>();
    LOG_INFO("[NETPIE] Action received: %s", action.c_str());

    if (action == "dispense") {
      int slot = doc["slot"].as<int>(); // 0-5
      if (slot >= 0 && slot <= 5) {
        LOG_INFO("[NETPIE] Dispense Command for Slot %d", slot);
        if (onNetpieDispense) {
          onNetpieDispense(slot);
        } else {
          LOG_ERROR("[NETPIE] No Dispense Callback set!");
        }
      } else {
        LOG_WARN("[NETPIE] Invalid slot: %d", slot);
      }
    }
  }

  // Handle Shadow/Schedule updates...

  // Debug: แสดง key ทั้งหมดใน data object
  if (doc["data"].is<JsonObject>()) {
    JsonObject data = doc["data"];
    LOG_INFO("[NETPIE] --- data keys ---");
    for (JsonPair kv : data) {
      // แสดง key + value (ตัดที่ 60 ตัวอักษร)
      String val;
      if (kv.value().is<String>()) {
        val = kv.value().as<String>();
      } else if (kv.value().is<int>()) {
        val = String(kv.value().as<int>());
      } else if (kv.value().is<float>()) {
        val = String(kv.value().as<float>(), 2);
      } else {
        val = "(object/array)";
      }
      LOG_INFO("[NETPIE]   %s = %s", kv.key().c_str(), val.c_str());
    }
    LOG_INFO("[NETPIE] --- end keys ---");
  }

  // ตรวจสอบคำสั่ง Dispense (รองรับทั้ง dispenseCommand และ dispenseNow)
  if (doc["action"] == "dispense" || doc["data"]["dispenseCommand"] == 1 ||
      doc["data"]["dispenseNow"] == 1) {
    LOG_INFO("[NETPIE] >>> DISPENSE COMMAND received");
    if (onDispenseCommand) {
      onDispenseCommand("NETPIE Command");
    }
    return;
  }

  // ตรวจสอบการอัปเดตตารางเวลา
  if (doc["data"].is<JsonObject>() && onScheduleUpdate) {
    LOG_INFO("[NETPIE] >>> Schedule Update - forwarding to main");
    onScheduleUpdate(doc);
  }
}

// --- Setup ---
void netpieSetup() {
  mqttClient.setBufferSize(
      2048); // ต้องเพิ่ม! default 256 bytes ไม่พอสำหรับ shadow 26 keys
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  LOG_INFO("[NETPIE] Setup complete (buffer: 2048)");
}

// --- Connect ---
static bool shadowRequested = false; // Flag เพื่อขอ shadow แค่ครั้งแรก

void netpieConnect() {
  if (WiFi.status() != WL_CONNECTED)
    return;
  if (mqttClient.connected())
    return;

  LOG_INFO("[NETPIE] Connecting...");

  // Clean session = true เพื่อหลีกเลี่ยง message ค้าง
  if (mqttClient.connect(MQTT_CLIENT_ID, MQTT_TOKEN, MQTT_SECRET)) {
    LOG_INFO("[NETPIE] Connected!");
    netpieConnected = true;

    // Subscribe topics
    mqttClient.subscribe("@shadow/data/updated");
    mqttClient.subscribe("@shadow/data/get/response");
    mqttClient.subscribe("@msg/command");

    // Request shadow เฉพาะครั้งแรกเท่านั้น
    if (!shadowRequested) {
      LOG_INFO("[NETPIE] Requesting initial shadow...");
      mqttClient.publish("@shadow/data/get", "");
      shadowRequested = true;
    }

  } else {
    LOG_ERROR("[NETPIE] Failed, rc=%d", mqttClient.state());
    netpieConnected = false;
  }
}

// --- Loop (non-blocking) ---
void netpieLoop() {
  // Allow immediate connect on first run (or if disconnected for a while)
  static unsigned long lastReconnect = 0;
  static bool firstRun = true;

  if (!mqttClient.connected()) {
    netpieConnected = false;

    // Retry every 5s, but try immediately on first run if WiFi is ready
    if (firstRun || millis() - lastReconnect > 5000) {
      if (WiFi.status() == WL_CONNECTED) {
        netpieConnect();
        lastReconnect = millis();
        firstRun = false;
      }
    }
  } else {
    mqttClient.loop();
  }
}

// --- Update Shadow ---
void updateShadow(String status, String detail) {
  if (!mqttClient.connected())
    return;

  static char buffer[512]; // Bug10 fix: เพิ่ม buffer สำหรับข้อความภาษาไทย (UTF-8)
  snprintf(buffer, sizeof(buffer),
           "{\"data\":{\"status\":\"%s\",\"detail\":\"%s\"}}", status.c_str(),
           detail.c_str());

  mqttClient.publish("@shadow/data/update", buffer);
  LOG_DEBUG("[NETPIE] Shadow updated: %s", status.c_str());
}

void updateShadowKey(String key, String value) {
  if (!mqttClient.connected())
    return;

  static char buffer[128];
  snprintf(buffer, sizeof(buffer), "{\"data\":{\"%s\":\"%s\"}}", key.c_str(),
           value.c_str());
  mqttClient.publish("@shadow/data/update", buffer);
  LOG_INFO("[NETPIE] Shadow Key updated: %s = %s", key.c_str(), value.c_str());
}

void updateShadowKey(String key, int value) {
  if (!mqttClient.connected())
    return;

  static char buffer[128];
  snprintf(buffer, sizeof(buffer), "{\"data\":{\"%s\":%d}}", key.c_str(),
           value);
  mqttClient.publish("@shadow/data/update", buffer);
  LOG_INFO("[NETPIE] Shadow Key updated: %s = %d", key.c_str(), value);
}
