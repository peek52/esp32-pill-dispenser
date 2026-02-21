#pragma once
#include "config.h"
#include <Arduino.h>

// ============================================================
// Servo Control via PCA9685
// ============================================================
void servoSetup(void);
void servoDispense(int slot);
void servoToggleManual(int moduleIndex);
bool servoIsManualActive(int moduleIndex);
void servoHome(void);
