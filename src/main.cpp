#include "config.h"
#include "display_module.h"
#include "netpie.h"         // Internet
#include "offline_module.h" // Offline logging
#include "scheduler.h"
#include "servo_control.h"
#include "telegram.h"          // Internet
#include "telegram_messages.h" // Internet
#include "ui_manager.h"
#include "wifi_manager.h"
#include <Arduino.h>
#include <Network.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>


// ============================================================
// Forward declarations
// ============================================================
void onNetpieDispenseSlot(int moduleIndex);

// ============================================================
// Display / Dispense Logic
// ============================================================
void onConfirmedDispense(int timeSlotIndex) {
  Serial.printf("[Main] User confirmed dispense for slot %d\n", timeSlotIndex);

  int dispensed = 0;
  String medNames = "";

  for (int m = 0; m < NUM_MODULES; m++) {
    MedModule &mod = moduleGet(m);
    if (mod.slotMask & (1 << timeSlotIndex)) {
      Serial.printf("[Main] Dispensing module %d (%s)\n", m, mod.name);

      // Show dispensing animation
      uiShowDispensing(m);

      // Activate servo
      servoDispense(m);

      // Decrement qty
      if (mod.qty > 0) {
        mod.qty--;
        updateShadowKey("med" + String(m + 1) + "_count", mod.qty);
      }

      medNames += String(mod.name) + " ";
      dispensed++;
    }
  }

  // Save updated quantities
  if (dispensed > 0) {
    schedulerSave();
    uiShowResult(timeSlotIndex, true);
    if (wifiIsConnected()) {
      sendTelegramNotification(String(MSG_DISPENSE_SUCCESS) +
                               "\nรายการ: " + medNames + "\n(รับยาหน้าเครื่อง)");
    }
    updateShadowOfflineAware("success",
                             "Dispensed from slot " + String(timeSlotIndex));
  } else {
    Serial.println("[Main] No modules assigned to this slot");
    uiShowResult(timeSlotIndex, false);
  }
}

// ============================================================
// Scheduled Trigger Callback
// ============================================================
void onDispenseTrigger(int timeSlotIndex) {
  Serial.printf("[Main] Time slot %d triggered\n", timeSlotIndex);

  int assignedCount = 0;
  String pendingMeds = "";
  for (int m = 0; m < NUM_MODULES; m++) {
    MedModule &mod = moduleGet(m);
    if (mod.slotMask & (1 << timeSlotIndex)) {
      assignedCount++;
      if (mod.qty > 0)
        pendingMeds += String(mod.name) + ", ";
    }
  }

  if (assignedCount > 0) {
    // Wait for user confirmation instead of dispensing immediately
    uiShowConfirmDispense(timeSlotIndex);

    // Telegram Notification
    if (wifiIsConnected()) {
      const char *slotLabels[] = {"เช้าก่อนอาหาร",  "เช้าหลังอาหาร", "เที่ยงก่อนอาหาร",
                                  "เที่ยงหลังอาหาร", "เย็นก่อนอาหาร", "เย็นหลังอาหาร",
                                  "ก่อนนอน"};
      String msg =
          "⏰ ถึงเวลาทานยาแล้วครับ (" + String(slotLabels[timeSlotIndex]) + ")\n";
      msg += "💊 รายการ: " + pendingMeds + "\n";
      msg += "✅ กรุณากดปุ่มที่หน้าจอเพื่อรับยา";
      sendTelegramNotification(msg);
      updateShadowOfflineAware("waiting_confirm",
                               "Time slot " + String(timeSlotIndex));
    }
  } else {
    Serial.println("[Main] No modules assigned to this slot (ignored)");
  }
}

// ============================================================
// Manual dispense via UI — toggles ONE specific module servo
// ============================================================
void onManualDispense(int moduleIndex) {
  if (moduleIndex < 0 || moduleIndex >= NUM_MODULES)
    return;
  MedModule &mod = moduleGet(moduleIndex);
  Serial.printf("[Main] Manual servo toggle for module %d (%s)\n", moduleIndex,
                mod.name);
  servoToggleManual(moduleIndex);
}

// ============================================================
// Remote Dispense (NETPIE / Telegram)
// ============================================================
void onNetpieDispenseSlot(int moduleIndex) {
  if (moduleIndex >= 0 && moduleIndex < NUM_MODULES) {
    uiShowDispensing(moduleIndex);
    servoDispense(moduleIndex);

    MedModule &mod = moduleGet(moduleIndex);
    if (mod.qty > 0) {
      mod.qty--;
      schedulerSave();
      updateShadowKey("med" + String(moduleIndex + 1) + "_count", mod.qty);
    }

    updateShadowOfflineAware("success", "Remote Dispense: " + String(mod.name));
    if (wifiIsConnected()) {
      sendTelegramNotification(String(MSG_DISPENSE_SUCCESS) + "\n" +
                               String(mod.name) + " (สั่งงานระยะไกล)");
    }

    // Jump back to home screen after Dispensing UI closes
    uiShowResult(0, true);
  }
}

void onDispenseCommand(String source) {
  // Generic dispense command - dispense all assigned to current time maybe?
  Serial.println("[Main] Generic dispense command from: " + source);
}

// ============================================================
// Sync Schedule from NETPIE
// ============================================================
void onScheduleUpdate(JsonDocument &doc) {
  Serial.println("[Main] Processing Schedule from NETPIE...");
  JsonObject data = doc["data"];

  // 1. Time slots
  String timeKeys[] = {"t_morn_pre", "t_morn_post", "t_noon_pre", "t_noon_post",
                       "t_eve_pre",  "t_eve_post",  "t_bed"};
  for (int i = 0; i < 7; i++) {
    if (data[timeKeys[i]].is<String>()) {
      String timeStr = data[timeKeys[i]].as<String>();
      if (timeStr.length() >= 5) {
        uint8_t h = timeStr.substring(0, 2).toInt();
        uint8_t m = timeStr.substring(3, 5).toInt();
        TimeSlot &currentTs = timeSlotGet(i);
        timeSlotSet(i, h, m, currentTs.enabled);
      }
    }
  }

  // 2. Modules
  for (int i = 0; i < NUM_MODULES; i++) {
    String p = "med" + String(i + 1);
    if (data[p + "_name"].is<String>()) {
      moduleSetName(i, data[p + "_name"].as<String>().c_str());
    }
    if (data[p + "_count"].is<int>()) {
      moduleSetQty(i, data[p + "_count"].as<int>());
    }
    if (data[p + "_slots"].is<int>()) {
      moduleSetSlotMask(i, data[p + "_slots"].as<int>());
    }
  }

  // 3. Global Enable
  if (data["scheduleEnabled"].is<int>()) {
    schedulerSetEnabled(data["scheduleEnabled"].as<int>() == 1);
  }

  schedulerSave();
  Serial.println("[Main] Schedule updated & saved!");
}

// ============================================================
// Telegram text commands
// ============================================================
void processTelegramCommand(String command) {
  command.toLowerCase();

  if (command == "/status" || command == "สถานะ") {
    String msg = String(MSG_SYSTEM_STATUS) + "\n";
    uint8_t h, m, s;
    schedulerGetTime(h, m, s);
    char tbuf[10];
    sprintf(tbuf, "%02d:%02d", h, m);
    msg += "เวลา: " + String(tbuf) + "\n";

    for (int i = 0; i < NUM_MODULES; i++) {
      MedModule &mod = moduleGet(i);
      msg += String(i + 1) + ". " + String(mod.name) + " (" + String(mod.qty) +
             " เม็ด)\n";
    }
    msg += String(MSG_LABEL_WIFI) + wifiGetSSID();
    sendTelegramNotification(msg);

  } else if (command.startsWith("/dispense") || command.startsWith("จ่ายยา")) {
    int spaceIndex = command.indexOf(' ');
    if (spaceIndex != -1) {
      int modIdx = command.substring(spaceIndex + 1).toInt() - 1;
      onNetpieDispenseSlot(modIdx);
    }
  } else if (command == "/gettime" || command == "ดูเวลา") {
    String msg = "📅 ตารางเวลาปัจจุบัน:\n";
    const char *slotLabels[] = {"เช้าก่อน", "เช้าหลัง", "เที่ยงก่อน", "เที่ยงหลัง",
                                "เย็นก่อน", "เย็นหลัง", "ก่อนนอน"};
    for (int i = 0; i < NUM_TIME_SLOTS; i++) {
      TimeSlot &ts = timeSlotGet(i);
      char tbuf[10];
      sprintf(tbuf, "%02d:%02d", ts.hour, ts.minute);
      msg += String(i + 1) + ". " + String(slotLabels[i]) + ": " +
             String(tbuf) + (ts.enabled ? " (ON)\n" : " (OFF)\n");
    }
    sendTelegramNotification(msg);
  } else {
    // defaults to help
    String msg = String(MSG_COMMAND_LIST) + "\n";
    msg += "/dispense [1-6] - จ่ายยาตลับ N\n";
    msg += "/status - ดูสถานะยา\n";
    msg += "/gettime - ดูตารางเวลา\n";
    sendTelegramNotification(msg);
  }
}

// ============================================================
// Setup
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Medicine Dispenser Starting ===");

  // I2C Bus for UI Touch & Servos & RTC
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
  Serial.printf("[I2C] Bus OK — SDA=%d, SCL=%d\n", I2C_SDA_PIN, I2C_SCL_PIN);

  // GPIO
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Core Subsystems
  displaySetup();
  servoSetup();
  wifiSetup();
  schedulerSetup();
  schedulerSetCallback(onDispenseTrigger);

  // UI Setup
  uiSetup();
  uiSetManualDispenseCallback(onManualDispense);
  uiSetConfirmDispenseCallback(onConfirmedDispense);

  // Internet & Logging Setup
  offlineSetup();
  netpieSetup();
  setScheduleCallback(onScheduleUpdate);
  setDispenseCallback(onDispenseCommand);
  setNetpieDispenseCallback(onNetpieDispenseSlot);

  telegramSetup();
  setTelegramCommandCallback(processTelegramCommand);

  Serial.println("=== Setup Complete ===");
}

// ============================================================
// Main Loop
// ============================================================
void loop() {
  uiLoop();
  schedulerLoop();
  wifiLoop();

  // Run internet tasks if connected
  if (wifiIsConnected()) {
    netpieLoop();
    telegramLoop();
  }

  delay(10);
}
