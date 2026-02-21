#pragma once
#include <Arduino.h>

// ============================================================
// UI Screen States
// ============================================================
enum Screen {
  SCREEN_HOME,             // Clock + next schedule + buttons
  SCREEN_SCHEDULE,         // 4 time periods (2x2 grid)
  SCREEN_TIME_PICKER,      // Edit hour:minute for one slot
  SCREEN_MODULES,          // 6 module cards
  SCREEN_MODULE_DETAIL,    // Edit single module (name, qty, slots)
  SCREEN_MANUAL_DISPENSE,  // Grid of modules to manually dispense
  SCREEN_CONFIRM_DISPENSE, // Waiting for user tap to confirm dispense
  SCREEN_WIFI_MENU,        // Choose WiFi connect method
  SCREEN_WIFI_SCAN,        // Show available networks
  SCREEN_WIFI_OSK,         // On-Screen Keyboard for password
  SCREEN_WIFI_PORTAL,      // Showing instructions for Captive Portal
  SCREEN_DISPENSING,
  SCREEN_RESULT
};

// ============================================================
// Public API
// ============================================================
void uiSetup(void);
void uiLoop(void);
void uiShowDispensing(int moduleIndex);
void uiShowResult(int timeSlotIndex, bool success);
void uiShowConfirmDispense(int timeSlotIndex);

// Callback for manual dispense
typedef void (*ManualDispenseCallback)(int moduleIndex);
void uiSetManualDispenseCallback(ManualDispenseCallback cb);

// Callback for confirming scheduled dispense
typedef void (*ConfirmDispenseCallback)(int timeSlotIndex);
void uiSetConfirmDispenseCallback(ConfirmDispenseCallback cb);
