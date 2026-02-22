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
#include "audio_manager.h" // Added by instruction
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
// Dispense State Machine (Non-blocking)
// ============================================================
enum DispenseState {
  STATE_IDLE,
  STATE_START_DISPENSE,
  STATE_WAIT_SERVO,
  STATE_SHOW_RESULT,
  STATE_WAIT_RESULT
};

struct DispenseJob {
  bool active;
  DispenseState state;
  int timeSlotIndex; // -1 if remote single module
  int currentModule;
  unsigned long timer;
};

static DispenseJob dJob = {false, STATE_IDLE, -1, 0, 0};

void processDispenseJob() {
  if (!dJob.active)
    return;

  switch (dJob.state) {
  case STATE_START_DISPENSE:
    if (dJob.timeSlotIndex != -1) {
      while (dJob.currentModule < NUM_MODULES) {
        MedModule &mod = moduleGet(dJob.currentModule);
        if ((mod.slotMask & (1 << dJob.timeSlotIndex)) && mod.qty > 0) {
          break;
        }
        dJob.currentModule++;
      }
    }

    if (dJob.currentModule >= NUM_MODULES) {
      dJob.state = STATE_SHOW_RESULT;
    } else {
      uiSetDispensingScreen(dJob.currentModule);
      servoStartDispense(dJob.currentModule);
      dJob.state = STATE_WAIT_SERVO;
    }
    break;

  case STATE_WAIT_SERVO: {
    DispenseStatus st = servoGetDispenseStatus();
    if (st == DISPENSE_BUSY)
      break; // still dispensing

    MedModule &mod = moduleGet(dJob.currentModule);

    if (st == DISPENSE_SUCCESS) {
      if (mod.qty > 0) {
        mod.qty--;
        String logData = (dJob.timeSlotIndex != -1)
                             ? ("slot:" + String(dJob.timeSlotIndex) +
                                ",module:" + String(dJob.currentModule))
                             : ("remote,module:" + String(dJob.currentModule));
        addToOfflineQueue("dispense", logData);
        updateShadowKey("med" + String(dJob.currentModule + 1) + "_count",
                        mod.qty);
      }
    } else { // DISPENSE_EMPTY (Failed after retries)
      String logData = "error_jammed,module:" + String(dJob.currentModule);
      addToOfflineQueue("error", logData);
      updateShadowOfflineAware("error", "Module " +
                                            String(dJob.currentModule + 1) +
                                            " Dispense Failed!");
      if (wifiIsConnected()) {
        sendTelegramNotification("⚠️ แจ้งเตือน: เกิดข้อผิดพลาด! ยาในกล่อง [" +
                                 String(mod.name) +
                                 "] จ่ายไม่ออกหรือเกิดการติดขัด (Dispense Failed)");
      }
    }

    if (dJob.timeSlotIndex != -1) {
      dJob.currentModule++;
      dJob.state = STATE_START_DISPENSE;
    } else {
      if (st == DISPENSE_SUCCESS) {
        updateShadowOfflineAware("success",
                                 "Remote Dispense: " + String(mod.name));
        if (wifiIsConnected()) {
          sendTelegramNotification(String(MSG_DISPENSE_SUCCESS) + "\n" +
                                   String(mod.name) + " (สั่งงานระยะไกล)");
        }
      }
      dJob.currentModule = NUM_MODULES; // Force end
      dJob.state = STATE_SHOW_RESULT;
    }
  } break;

  case STATE_SHOW_RESULT:
    schedulerSave();
    updateShadowOfflineAware("status", "idle");
    updateShadowOfflineAware("waiting_confirm", "");
    uiSetResultScreen(true);
    dJob.timer = millis();
    dJob.state = STATE_WAIT_RESULT;
    break;

  case STATE_WAIT_RESULT:
    if (millis() - dJob.timer >= 3000) {
      uiGoHome();
      dJob.active = false;
      dJob.state = STATE_IDLE;
    }
    break;

  case STATE_IDLE:
    break;
  }
}

void onConfirmedDispense(int timeSlotIndex) {
  Serial.printf("[Main] User confirmed dispense for slot %d\n", timeSlotIndex);
  if (dJob.active)
    return;

  dJob.active = true;
  dJob.timeSlotIndex = timeSlotIndex;
  dJob.currentModule = 0;
  dJob.state = STATE_START_DISPENSE;

  updateShadowOfflineAware("status", "dispensing");
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
    // Play dispense notification sound
    audioPlay(2);

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
    if (dJob.active)
      return;

    MedModule &mod = moduleGet(moduleIndex);
    if (mod.qty <= 0) {
      Serial.printf("[Main] Module %d is empty, ignoring remote dispense\n",
                    moduleIndex);
      return; // Cannot dispense an empty module
    }

    dJob.active = true;
    dJob.timeSlotIndex = -1;
    dJob.currentModule = moduleIndex;
    dJob.state = STATE_START_DISPENSE;
    updateShadowOfflineAware("status", "dispensing");
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
// ============================================================
// Network Task (Core 0)
// ============================================================
void networkTask(void *pvParameters) {
  for (;;) {
    if (wifiIsConnected()) {
      netpieLoop();
      telegramLoop();
    }
    vTaskDelay(pdMS_TO_TICKS(50)); // Yield to other tasks
  }
}

// ============================================================
// Setup
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Medicine Dispenser Starting ===");

  // GPIO
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // 1. Core Subsystems - Display First
  displaySetup();

  // 2. I2C Bus for Servos & RTC (LovyanGFX already set up the pins internally)
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
  Serial.printf("[I2C] Bus Attached — SDA=%d, SCL=%d\n", I2C_SDA_PIN,
                I2C_SCL_PIN);

  // 3. Other Core Subsystems
  servoSetup();
  wifiSetup();
  schedulerSetup();
  schedulerSetCallback(onDispenseTrigger);
  schedulerSetWarnCallback([](int slot) { audioPlay(1); });
  audioSetup();

  // UI Setup
  uiSetup();
  uiSetManualDispenseCallback(onManualDispense);
  uiSetConfirmDispenseCallback(onConfirmedDispense);
  uiSetNameChangeCallback([](int moduleIndex, const char *newName) {
    updateShadowKey("med" + String(moduleIndex + 1) + "_name", String(newName));
  });

  // Internet & Logging Setup
  offlineSetup();
  netpieSetup();
  setScheduleCallback(onScheduleUpdate);
  setDispenseCallback(onDispenseCommand);
  setNetpieDispenseCallback(onNetpieDispenseSlot);

  telegramSetup();
  setTelegramCommandCallback(processTelegramCommand);

  // Start Network Task on Core 0
  xTaskCreatePinnedToCore(networkTask,   /* Task function. */
                          "NetworkTask", /* name of task. */
                          16384,         /* Stack size of task */
                          NULL,          /* parameter of the task */
                          1,             /* priority of the task */
                          NULL,          /* Task handle */
                          0);            /* pin task to core 0 */

  Serial.println("=== Setup Complete ===");
}

// ============================================================
// Main Loop
// ============================================================
void loop() {
  uiLoop();
  schedulerLoop();
  wifiLoop();

  // Network loops (Netpie/Telegram) are now handled in networkTask on Core 0

  servoLoop();
  processDispenseJob();

  // ============================================================
  // Physical Button Logic
  // Short Press: Dispense Module 0
  // Long Press (5s): Reset WiFi credentials
  // ============================================================
  static unsigned long btnPressStart = 0;
  static bool btnIsPressed = false;
  static bool longPressHandled = false;
  static unsigned long lastDebounceTime = 0;
  static int lastBtnState = HIGH;

  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastBtnState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > 50) {
    // State has stabilized
    if (reading == LOW && !btnIsPressed) {
      // Button just went down
      btnIsPressed = true;
      btnPressStart = millis();
      longPressHandled = false;
    } else if (reading == LOW && btnIsPressed) {
      // Button is being held down
      if (!longPressHandled && (millis() - btnPressStart > 5000)) {
        Serial.println("[Button] Long press detected - Forgetting WiFi!");
        wifiForget();
        longPressHandled = true;
      }
    } else if (reading == HIGH && btnIsPressed) {
      // Button just released
      if (!longPressHandled && (millis() - btnPressStart > 50)) {
        Serial.println("[Button] Short press detected - Manual Dispense!");
        onManualDispense(0);
      }
      btnIsPressed = false;
    }
  }
  lastBtnState = reading;

  delay(10);
}
