#include "servo_control.h"
#include <Adafruit_PWMServoDriver.h>
#include <Wire.h>

// ============================================================
// PCA9685 Servo Driver (I2C)
// ============================================================
static Adafruit_PWMServoDriver pca9685(PCA9685_ADDR);
static bool pcaFound = false;

#define SERVO_MIN_PULSE 150
#define SERVO_MAX_PULSE 600

static uint16_t angleToPulse(int angle) {
  return map(angle, 0, 180, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
}

// ============================================================
void servoSetup(void) {
  // Re-init Wire after LovyanGFX touch init
  Wire.end();
  delay(100);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  delay(100);

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
void servoDispense(int moduleIndex) {
  if (!pcaFound) {
    Serial.println("[Servo] PCA9685 not available");
    return;
  }

  int ch = moduleIndex % 16;
  Serial.printf("[Servo] Dispensing module=%d ch=%d\n", moduleIndex, ch);

  // Auto dispense sequence
  pca9685.setPWM(ch, 0, angleToPulse(SERVO_ANGLE_DISP));
  delay(800);

  pca9685.setPWM(ch, 0, angleToPulse(SERVO_ANGLE_HOME));
  delay(500);

  pca9685.setPWM(ch, 0, 0);
  manualServoState[moduleIndex] =
      false; // Reset toggle state if it was moved automatically
  Serial.println("[Servo] Done");
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
    pca9685.setPWM(ch, 0, angleToPulse(SERVO_ANGLE_DISP));
  } else {
    // Return to home position
    pca9685.setPWM(ch, 0, angleToPulse(SERVO_ANGLE_HOME));
    // Optional: delay then release PWM to save power like in home()
    // delay(300);
    // pca9685.setPWM(ch, 0, 0);
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
    pca9685.setPWM(i, 0, angleToPulse(SERVO_ANGLE_HOME));
  }
  delay(300);
  for (int i = 0; i < NUM_MODULES; i++) {
    pca9685.setPWM(i, 0, 0);
  }
}
