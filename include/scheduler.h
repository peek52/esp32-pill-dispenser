#pragma once
#include "config.h"
#include <Arduino.h>

// ============================================================
// Time Slot — one of 7 daily dispense times
// ============================================================
struct TimeSlot {
  uint8_t hour;
  uint8_t minute;
  bool enabled;
};

// ============================================================
// Medicine Module — one of 6 physical pill compartments
// ============================================================
struct MedModule {
  char name[MAX_MED_NAME]; // e.g. "พารา", "วิตามิน"
  uint8_t qty;             // remaining pills (0-99)
  uint8_t slotMask;        // bitmask: which time slots to dispense
                           // bit 0 = เช้าก่อน, bit 1 = เช้าหลัง, ...
                           // bit 6 = ก่อนนอน
};

// ============================================================
// Public API
// ============================================================
void schedulerSetup(void);
void schedulerLoop(void);

// --- Time Slot CRUD ---
TimeSlot &timeSlotGet(int index);
void timeSlotSet(int index, uint8_t h, uint8_t m, bool en);

// --- Module CRUD ---
MedModule &moduleGet(int index);
void moduleSetName(int index, const char *name);
void moduleSetQty(int index, uint8_t qty);
void moduleSetSlotMask(int index, uint8_t mask);
void moduleToggleSlot(int index, int slotBit);

// --- Persistence ---
void schedulerSave(void);
void schedulerLoad(void);

// --- RTC Time ---
void schedulerGetTime(uint8_t &h, uint8_t &m, uint8_t &s);
void schedulerGetDate(uint16_t &year, uint8_t &month, uint8_t &day,
                      uint8_t &dow);
uint32_t schedulerGetUnixTime(void);
bool schedulerHasRTC(void);

// --- Next upcoming slot ---
int schedulerNextSlot(void); // returns slot index (0-6) or -1

// --- Callback for dispense trigger ---
typedef void (*SchedDispenseCallback)(int timeSlotIndex);
void schedulerSetCallback(SchedDispenseCallback cb);

// --- Schedule master enable/disable ---
bool schedulerIsEnabled(void);
void schedulerSetEnabled(bool en);
