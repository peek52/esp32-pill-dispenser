#pragma once
#include "config.h"
#include <Arduino.h>

// ============================================================
// Servo Control via PCA9685
// ============================================================
enum DispenseStatus { DISPENSE_BUSY, DISPENSE_SUCCESS, DISPENSE_EMPTY };

void servoSetup(void);
void servoLoop(void);
bool servoStartDispense(int moduleIndex);
DispenseStatus servoGetDispenseStatus(void);
void servoToggleManual(int moduleIndex);
bool servoIsManualActive(int moduleIndex);
void servoHome(void);
