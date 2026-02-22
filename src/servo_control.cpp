#include "servo_control.h"
#include <Adafruit_PWMServoDriver.h>
#include <Arduino.h>
#include <Wire.h>

// ============================================================
// PCA9685 Servo Driver (I2C)
// ============================================================
static Adafruit_PWMServoDriver pca9685(PCA9685_ADDR);
static bool pcaFound = false;

// ============================================================
// PCF8574 IR Sensor (I2C)
// ============================================================
static bool pcfFound = false;

#define SERVO_MIN_PULSE 150
#define SERVO_MAX_PULSE 600

static uint16_t angleToPulse(int angle) {
  return map(angle, 0, 180, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
}

// ============================================================
// Robust PWM Setter to handle I2C collisions with Touch Panel
// ============================================================
static void safeSetPWM(int ch, uint16_t off) {
  if (!pcaFound)
    return;

  // Try sending the PWM command up to 5 times if the I2C bus is busy
  for (int attempt = 0; attempt < 5; attempt++) {
    Wire.beginTransmission(PCA9685_ADDR);
    // Write register 0x06 + 4*ch (LEDn_ON_L)
    Wire.write(0x06 + 4 * ch);
    Wire.write(0);          // ON_L
    Wire.write(0 >> 8);     // ON_H
    Wire.write(off & 0xFF); // OFF_L
    Wire.write(off >> 8);   // OFF_H

    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      // Success
      return;
    }
    Serial.printf("[Servo] I2C Collision, retry %d (err=%d)\n", attempt + 1,
                  err);
    delay(5); // Wait briefly before retrying
  }
  Serial.println("[Servo] FAILED to set PWM after 5 attempts!");
}

// ============================================================
void servoSetup(void) {
  delay(500); // Allow I2C bus to settle after LovyanGFX init

  // PCF8574 Init
  pcfFound = false;
  for (int attempt = 0; attempt < 3; attempt++) {
    Wire.beginTransmission(PCF8574_ADDR);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      pcfFound = true;
      break;
    }
    delay(200);
  }

  if (pcfFound) {
    Wire.beginTransmission(PCF8574_ADDR);
    Wire.write(0xFF); // Set all pins HIGH to read inputs
    Wire.endTransmission();
    Serial.println("[Servo] PCF8574 (IR) init OK");
  } else {
    Serial.println("[Servo] PCF8574 (IR) NOT found!");
  }

  // PCA9685 Init
  pcaFound = false;
  for (int attempt = 0; attempt < 3; attempt++) {
    Wire.beginTransmission(PCA9685_ADDR);
    uint8_t err = Wire.endTransmission();
    Serial.printf("[Servo] Probe PCA9685 (0x%02X) attempt %d: err=%d\n",
                  PCA9685_ADDR, attempt + 1, err);
    if (err == 0) {
      pcaFound = true;
      break;
    }
    delay(200);
  }

  if (pcaFound) {
    pca9685.begin();
    pca9685.setPWMFreq(SERVO_FREQ);
    delay(10);
    servoHome();
    Serial.println("[Servo] PCA9685 init OK");
  } else {
    Serial.println("[Servo] PCA9685 NOT found after 3 attempts");
  }
}

// ============================================================
// State for manual toggle
// ============================================================
static bool manualServoState[NUM_MODULES] = {false};

// ============================================================
// State machine for Async Dispense
// ============================================================
enum ServoState {
  SERVO_IDLE,
  SERVO_MOVING_DISPENSE,
  SERVO_MOVING_HOME,
  SERVO_WAIT_NEXT_RETRY
};

struct DispenseTask {
  bool active;
  ServoState state;
  unsigned long timer;
  int moduleIndex;
  int ch;
  int retryCount;
  bool pillDetected;
  DispenseStatus finalStatus;
};

static DispenseTask sTask = {false, SERVO_IDLE, 0,     -1,
                             -1,    0,          false, DISPENSE_BUSY};

void servoLoop(void) {
  if (!pcaFound)
    return;
  if (!sTask.active)
    return;

  unsigned long now = millis();

  // Safe I2C Polling for IR Sensor (PCF8574)
  static unsigned long lastSensorPoll = 0;
  if (pcfFound && !sTask.pillDetected &&
      (sTask.state == SERVO_MOVING_DISPENSE ||
       sTask.state == SERVO_MOVING_HOME)) {
    if (now - lastSensorPoll > 50) { // 50ms interval to prevent I2C flooding
      lastSensorPoll = now;
      Wire.requestFrom((uint16_t)PCF8574_ADDR, (uint8_t)1, true);
      if (Wire.available()) {
        uint8_t irState = Wire.read();
        // Assuming LOW means pill detected
        if ((irState & (1 << sTask.ch)) == 0) {
          sTask.pillDetected = true;
          Serial.printf("[Servo] Pill detected on ch %d!\n", sTask.ch);
        }
      }
    }
  }

  switch (sTask.state) {
  case SERVO_MOVING_DISPENSE:
    if (now - sTask.timer >=
        1200) { // Increased from 800 to ensure full movement
      safeSetPWM(sTask.ch, angleToPulse(SERVO_ANGLE_HOME));
      sTask.state = SERVO_MOVING_HOME;
      sTask.timer = now;
    }
    break;

  case SERVO_MOVING_HOME:
    if (now - sTask.timer >=
        1500) { // Increased from 500 to ensure the mechanism reaches HOME
                // completely before continuing
      safeSetPWM(sTask.ch, 0);

      if (sTask.pillDetected || !pcfFound) {
        sTask.finalStatus = DISPENSE_SUCCESS;
        manualServoState[sTask.moduleIndex] = false;
        sTask.active = false;
        sTask.state = SERVO_IDLE;
        Serial.println("[Servo] Done (Pill detected or no sensor)");
      } else {
        sTask.retryCount++;
        if (sTask.retryCount >= 3) {
          sTask.finalStatus = DISPENSE_EMPTY;
          manualServoState[sTask.moduleIndex] = false;
          sTask.active = false;
          sTask.state = SERVO_IDLE;
          Serial.printf("[Servo] FAILED: Module %d empty (no pill detected)\n",
                        sTask.moduleIndex);
        } else {
          Serial.printf("[Servo] Retry %d/3 for module %d\n", sTask.retryCount,
                        sTask.moduleIndex);
          sTask.state = SERVO_WAIT_NEXT_RETRY;
          sTask.timer = now;
        }
      }
    }
    break;

  case SERVO_WAIT_NEXT_RETRY:
    if (now - sTask.timer >= 1000) { // Wait 1s and try again
      safeSetPWM(sTask.ch, angleToPulse(SERVO_ANGLE_DISP));
      sTask.state = SERVO_MOVING_DISPENSE;
      sTask.timer = now;
    }
    break;

  case SERVO_IDLE:
    break;
  }
}

bool servoStartDispense(int moduleIndex) {
  if (!pcaFound) {
    Serial.println("[Servo] ERRROR: Cannot dispense, PCA9685 not found!");
    return false;
  }
  if (sTask.active) {
    Serial.println("[Servo] WARNING: Cannot start dispense, already active!");
    return false; // Busy
  }

  int ch = moduleIndex % 16;
  Serial.printf("[Servo] Start Dispensing module=%d ch=%d\n", moduleIndex, ch);

  safeSetPWM(ch, angleToPulse(SERVO_ANGLE_DISP));
  sTask.active = true;
  sTask.state = SERVO_MOVING_DISPENSE;
  sTask.timer = millis();
  sTask.moduleIndex = moduleIndex;
  sTask.ch = ch;
  sTask.retryCount = 0;
  sTask.pillDetected = false;
  sTask.finalStatus = DISPENSE_BUSY;

  return true;
}

DispenseStatus servoGetDispenseStatus(void) {
  if (sTask.active)
    return DISPENSE_BUSY;
  return sTask.finalStatus;
}

// ============================================================
void servoToggleManual(int moduleIndex) {
  if (!pcaFound) {
    Serial.println("[Servo] PCA9685 not available");
    return;
  }

  int ch = moduleIndex % 16;
  manualServoState[moduleIndex] = !manualServoState[moduleIndex];

  Serial.printf("[Servo] Toggle module=%d ch=%d state=%s\n", moduleIndex, ch,
                manualServoState[moduleIndex] ? "DISPENSE" : "HOME");

  if (manualServoState[moduleIndex]) {
    // Move to dispense position and hold
    safeSetPWM(ch, angleToPulse(SERVO_ANGLE_DISP));
  } else {
    // Return to home position
    safeSetPWM(ch, angleToPulse(SERVO_ANGLE_HOME));
    // Optional: delay then release PWM to save power like in home()
    // delay(300);
    // safeSetPWM(ch, 0);
  }
}

// ============================================================
bool servoIsManualActive(int moduleIndex) {
  if (moduleIndex < 0 || moduleIndex >= NUM_MODULES)
    return false;
  return manualServoState[moduleIndex];
}

// ============================================================
void servoHome(void) {
  if (!pcaFound)
    return;
  for (int i = 0; i < NUM_MODULES; i++) {
    safeSetPWM(i, angleToPulse(SERVO_ANGLE_HOME));
  }
  delay(300);
  for (int i = 0; i < NUM_MODULES; i++) {
    safeSetPWM(i, 0);
  }
}
