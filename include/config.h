#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- Device Info ---
#define DEVICE_NAME "ESP32_Dispenser"
#define FIRMWARE_VERSION "3.0.0"

// --- I2C Bus (shared: PCA9685, PCF8574, DS3231, FT6236 Touch) ---
#define I2C_SDA_PIN 7
#define I2C_SCL_PIN 8

// --- I2C Device Addresses ---
#define PCA9685_ADDR 0x40 // Servo driver
#define PCF8574_ADDR 0x20 // IR sensor expander
#define FT6236_ADDR 0x38  // Capacitive touch controller

// --- SPI LCD (ST7796S 4.0" 480x320) ---
#define LCD_MOSI_PIN 32
#define LCD_MISO_PIN -1 // Not connected
#define LCD_SCK_PIN 36
#define LCD_CS_PIN 26
#define LCD_DC_PIN 24
#define LCD_RST_PIN 25

// --- Touch Panel (FT6236) ---
#define CTP_INT_PIN 21

// --- Button & LED ---
#define BUTTON_PIN 22 // Original was GPIO 22
#define LED_PIN 23    // Original was GPIO 23

// --- Display (Landscape Mode) ---
#define LCD_WIDTH 480
#define LCD_HEIGHT 320

// --- Time Slots (7 slots) ---
#define NUM_TIME_SLOTS 7

// --- Medicine Modules (6 slots on PCA9685 ch0-5) ---
#define NUM_MODULES 6
#define MAX_MED_NAME 16

// --- Servo ---
#define SERVO_ANGLE_HOME 27
#define SERVO_ANGLE_DISP 0
#define SERVO_FREQ 50

// --- Timings & Intervals ---
#define BOT_CHECK_INTERVAL 1000 // เช็ค Telegram ทุก 1 วิ

// --- MQTT (NETPIE) ---
#define MQTT_BROKER "mqtt.netpie.io"
#define MQTT_PORT 1883
// MQTT_CLIENT_ID, MQTT_TOKEN, MQTT_SECRET อยู่ใน secrets.h

// --- NTP Server ---
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 25200 // GMT+7 = 7 * 3600
#define DAYLIGHT_OFFSET_SEC 0

// --- Logging ---
#define LOG_LEVEL 4 // Debug Mode

// --- Debug Modes ---
#define TEST_IR_MODE false

#endif // CONFIG_H
