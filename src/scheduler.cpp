#include "scheduler.h"
#include <Preferences.h>
#include <RTClib.h>
#include <Wire.h>

// ============================================================
// RTC + Data Storage
// ============================================================
static RTC_DS3231 rtc;
static bool rtcFound = false;
static Preferences prefs;
static TimeSlot timeSlots[NUM_TIME_SLOTS];
static MedModule modules[NUM_MODULES];
static SchedDispenseCallback dispenseCallback = nullptr;
static SchedWarnCallback warnCallback = nullptr;
static bool dispensedToday[NUM_TIME_SLOTS] = {false};
static bool warnedToday[NUM_TIME_SLOTS] = {false};
static uint8_t lastMinute = 99;
static bool masterEnabled = true;

// Default time slot values
static const uint8_t defaultHours[NUM_TIME_SLOTS] = {8, 8, 12, 12, 17, 17, 21};
static const uint8_t defaultMins[NUM_TIME_SLOTS] = {0, 30, 0, 30, 0, 30, 0};

// ============================================================
// Setup
// ============================================================
void schedulerSetup(void) {
  if (rtc.begin()) {
    rtcFound = true;
    if (rtc.lostPower()) {
      Serial.println("[RTC] Lost power — setting to compile time");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
    DateTime now = rtc.now();
    Serial.printf("[RTC] OK — %04d-%02d-%02d %02d:%02d:%02d\n", now.year(),
                  now.month(), now.day(), now.hour(), now.minute(),
                  now.second());
  } else {
    Serial.println("[RTC] Not found — using millis fallback");
  }

  schedulerLoad();
}

// ============================================================
// Sync Time with NTP
// ============================================================
void schedulerSyncNTP(void) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 5000)) {
    Serial.println("[RTC] Failed to obtain time from NTP");
    return;
  }
  
  Serial.printf("[RTC] Received NTP Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

  if (rtcFound) {
    rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
                        timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min,
                        timeinfo.tm_sec));
    Serial.println("[RTC] DS3231 updated from NTP.");
  }
}

// ============================================================
// Loop — check if any time slot triggers
// ============================================================
void schedulerLoop(void) {
  if (!masterEnabled)
    return;
  if (millis() < 30000)
    return; // Grace period

  uint8_t h, m, s;
  schedulerGetTime(h, m, s);

  if (m == lastMinute)
    return;
  lastMinute = m;

  // Reset at midnight
  if (h == 0 && m == 0) {
    for (int i = 0; i < NUM_TIME_SLOTS; i++) {
      dispensedToday[i] = false;
      warnedToday[i] = false;
    }
  }

  // Check each time slot
  int nowMin = h * 60 + m;
  for (int i = 0; i < NUM_TIME_SLOTS; i++) {
    if (!timeSlots[i].enabled)
      continue;
      
    int slotMin = timeSlots[i].hour * 60 + timeSlots[i].minute;
    int diff = slotMin - nowMin;
    if (diff < 0) diff += 1440; // Handle day wrap
    
    // Warning 10 mins before
    if (diff == 10 && !warnedToday[i]) {
      warnedToday[i] = true;
      Serial.printf("[Scheduler] Warning slot %d (10 mins before)\n", i);
      if (warnCallback) {
        warnCallback(i);
      }
    }

    if (!dispensedToday[i]) {
      if (timeSlots[i].hour == h && timeSlots[i].minute == m) {
        dispensedToday[i] = true;
        warnedToday[i] = true; // In case we missed the warning window
        Serial.printf("[Scheduler] Trigger slot %d at %02d:%02d\n", i, h, m);
        if (dispenseCallback) {
          dispenseCallback(i);
        }
      }
    }
  }
}

void schedulerSetWarnCallback(SchedWarnCallback cb) { warnCallback = cb; }

// ============================================================
// Time Slot CRUD
// ============================================================
TimeSlot &timeSlotGet(int index) { return timeSlots[index % NUM_TIME_SLOTS]; }

void timeSlotSet(int index, uint8_t h, uint8_t m, bool en) {
  if (index < 0 || index >= NUM_TIME_SLOTS)
    return;
  timeSlots[index].hour = h;
  timeSlots[index].minute = m;
  timeSlots[index].enabled = en;
}

// ============================================================
// Module CRUD
// ============================================================
MedModule &moduleGet(int index) { return modules[index % NUM_MODULES]; }

void moduleSetName(int index, const char *name) {
  if (index < 0 || index >= NUM_MODULES)
    return;
  strncpy(modules[index].name, name, MAX_MED_NAME - 1);
  modules[index].name[MAX_MED_NAME - 1] = '\0';
}

void moduleSetQty(int index, uint8_t qty) {
  if (index < 0 || index >= NUM_MODULES)
    return;
  modules[index].qty = qty;
}

void moduleSetSlotMask(int index, uint8_t mask) {
  if (index < 0 || index >= NUM_MODULES)
    return;
  modules[index].slotMask = mask;
}

void moduleToggleSlot(int index, int slotBit) {
  if (index < 0 || index >= NUM_MODULES)
    return;
  if (slotBit < 0 || slotBit >= NUM_TIME_SLOTS)
    return;
  modules[index].slotMask ^= (1 << slotBit);
}

// ============================================================
// Persistence — NVS
// ============================================================
void schedulerSave(void) {
  prefs.begin("sched2", false);

  // Save master enable
  prefs.putBool("masterEn", masterEnabled);

  // Save time slots
  for (int i = 0; i < NUM_TIME_SLOTS; i++) {
    String key = "ts" + String(i);
    uint32_t val = (timeSlots[i].hour << 16) | (timeSlots[i].minute << 8) |
                   timeSlots[i].enabled;
    prefs.putUInt(key.c_str(), val);
  }

  // Save modules
  for (int i = 0; i < NUM_MODULES; i++) {
    String keyN = "mn" + String(i);
    String keyQ = "mq" + String(i);
    String keyS = "ms" + String(i);
    prefs.putString(keyN.c_str(), modules[i].name);
    prefs.putUChar(keyQ.c_str(), modules[i].qty);
    prefs.putUChar(keyS.c_str(), modules[i].slotMask);
  }

  prefs.end();
  Serial.println("[Scheduler] Saved to NVS");
}

void schedulerLoad(void) {
  prefs.begin("sched2", true);

  masterEnabled = prefs.getBool("masterEn", true);

  // Load time slots
  for (int i = 0; i < NUM_TIME_SLOTS; i++) {
    String key = "ts" + String(i);
    uint32_t val = prefs.getUInt(key.c_str(), 0xFFFFFFFF);
    if (val == 0xFFFFFFFF) {
      // Default values
      timeSlots[i].hour = defaultHours[i];
      timeSlots[i].minute = defaultMins[i];
      timeSlots[i].enabled = true;
    } else {
      timeSlots[i].hour = (val >> 16) & 0xFF;
      timeSlots[i].minute = (val >> 8) & 0xFF;
      timeSlots[i].enabled = val & 0x01;
      if (timeSlots[i].hour > 23)
        timeSlots[i].hour = defaultHours[i];
      if (timeSlots[i].minute > 59)
        timeSlots[i].minute = defaultMins[i];
    }
  }

  // Load modules
  for (int i = 0; i < NUM_MODULES; i++) {
    String keyN = "mn" + String(i);
    String keyQ = "mq" + String(i);
    String keyS = "ms" + String(i);
    String name = prefs.getString(keyN.c_str(), "");
    if (name.length() == 0) {
      snprintf(modules[i].name, MAX_MED_NAME, "ตลับ %d", i + 1);
    } else {
      strncpy(modules[i].name, name.c_str(), MAX_MED_NAME - 1);
      modules[i].name[MAX_MED_NAME - 1] = '\0';
    }
    modules[i].qty = prefs.getUChar(keyQ.c_str(), 0);
    modules[i].slotMask = prefs.getUChar(keyS.c_str(), 0);
  }

  prefs.end();
  Serial.println("[Scheduler] Loaded from NVS");
  for (int i = 0; i < NUM_TIME_SLOTS; i++) {
    Serial.printf("  TimeSlot %d: %02d:%02d %s\n", i, timeSlots[i].hour,
                  timeSlots[i].minute, timeSlots[i].enabled ? "ON" : "OFF");
  }
  for (int i = 0; i < NUM_MODULES; i++) {
    Serial.printf("  Module %d: \"%s\" qty=%d mask=0x%02X\n", i,
                  modules[i].name, modules[i].qty, modules[i].slotMask);
  }
}

// ============================================================
// Next upcoming slot
// ============================================================
int schedulerNextSlot(void) {
  uint8_t h, m, s;
  schedulerGetTime(h, m, s);
  int nowMin = h * 60 + m;

  int bestSlot = -1;
  int bestDist = 9999;

  for (int i = 0; i < NUM_TIME_SLOTS; i++) {
    if (!timeSlots[i].enabled)
      continue;
    int slotMin = timeSlots[i].hour * 60 + timeSlots[i].minute;
    int dist = slotMin - nowMin;
    if (dist <= 0)
      dist += 1440; // wrap to next day
    if (dist < bestDist) {
      bestDist = dist;
      bestSlot = i;
    }
  }
  return bestSlot;
}

// ============================================================
// RTC Time Access
// ============================================================
void schedulerGetTime(uint8_t &h, uint8_t &m, uint8_t &s) {
  if (rtcFound) {
    DateTime now = rtc.now();
    h = now.hour();
    m = now.minute();
    s = now.second();
  } else {
    unsigned long sec = millis() / 1000;
    h = (sec / 3600) % 24;
    m = (sec / 60) % 60;
    s = sec % 60;
  }
}

void schedulerGetDate(uint16_t &year, uint8_t &month, uint8_t &day,
                      uint8_t &dow) {
  if (rtcFound) {
    DateTime now = rtc.now();
    year = now.year();
    month = now.month();
    day = now.day();
    dow = now.dayOfTheWeek();
  } else {
    year = 2026;
    month = 1;
    day = 1;
    dow = 0;
  }
}

uint32_t schedulerGetUnixTime(void) {
  if (rtcFound) {
    return rtc.now().unixtime();
  }
  return millis() / 1000;
}

bool schedulerHasRTC(void) { return rtcFound; }
void schedulerSetCallback(SchedDispenseCallback cb) { dispenseCallback = cb; }
bool schedulerIsEnabled(void) { return masterEnabled; }
void schedulerSetEnabled(bool en) { masterEnabled = en; }

