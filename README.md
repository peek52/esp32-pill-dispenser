# ESP32 Pill Dispenser

A smart, connected medicine dispenser powered by an ESP32-P4-Nano microcontroller. This project integrates an offline touch-screen user interface with real-time internet connectivity via NETPIE MQTT and Telegram notifications.

## Features

- **Offline Touch UI:** A clean, user-friendly interface powered by LovyanGFX for configuring schedules and controlling the dispenser directly from a 4.0-inch ST7796S touchscreen.
- **Smart Scheduling:** Real-Time Clock (RTC DS3231) integration for accurate timekeeping and scheduling of medication times.
- **Hardware Integration:**
  - PCA9685 PWM Servo Driver for mechanically dispensing pills from up to 6 separate compartments.
  - PCF8574 I2C Expander for IR sensor feedback (detecting dispensed pills).
- **Internet Connectivity:**
  - **NETPIE Dashboard:** Syncs schedules and current status with the cloud using device shadows. Control the dispenser remotely through a web dashboard.
  - **Telegram Bot:** Receive real-time notifications when it's time to take medicine, when a dispense is successful, and use commands (e.g., `/status`, `/dispense`) to interact with the device remotely.
- **Persistent Storage:** Saves schedules and settings to the ESP32's NVS (Non-Volatile Storage) so data is not lost on power failure.
- **WiFi Manager:** Easy on-device WiFi configuration via a captive portal.

## Hardware Stack

- **Microcontroller:** Waveshare ESP32-P4-Nano
- **Display:** 4.0" ST7796S SPI LCD with FT6236 Capacitive Touch (Landscape mode)
- **RTC:** DS3231 (I2C)
- **Servo Driver:** PCA9685 (I2C)
- **IR Sensors Expander:** PCF8574 (I2C)

## Getting Started

### Prerequisites
- [PlatformIO Core](https://docs.platformio.org/en/latest/core/index.html) or the PlatformIO IDE extension for VSCode.

### Building and Uploading
1. Clone the repository.
2. Open the project in VSCode with the PlatformIO extension installed.
3. Configure your secrets in `include/secrets.h` (NETPIE credentials, Telegram Bot Token, etc.).
4. Click the PlatformIO **Upload** button to compile and upload the firmware to your ESP32-P4-Nano.

## Project Structure

- `src/main.cpp`: The core application brain that marries offline UI events with online callbacks.
- `src/ui_manager.cpp`: Handles all graphical user interface rendering and touch events.
- `src/display_module.cpp`: LovyanGFX display configuration and initialization.
- `src/scheduler.cpp`: RTC timekeeping, schedule checking, and NVS persistence logic.
- `src/servo_control.cpp`: PCA9685 setup and logic for actuating servo motors.
- `src/netpie.cpp`: MQTT setup and JSON serialization/deserialization for NETPIE shadow syncing.
- `src/telegram.cpp`: Telegram bot logic for polling commands and dispatching notifications.
- `src/wifi_manager.cpp`: Captive portal implementation for AP-based WiFi configuration.

## License
MIT License
