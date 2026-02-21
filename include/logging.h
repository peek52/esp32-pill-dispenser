#ifndef LOGGING_H
#define LOGGING_H

#include <Arduino.h>
#include "config.h"

// ============================================
// Logging Macros
// ============================================
// Usage: LOG_INFO("[Module] Message %d", value);

#if LOG_LEVEL >= 1
  #define LOG_ERROR(fmt, ...) Serial.printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#else
  #define LOG_ERROR(fmt, ...)
#endif

#if LOG_LEVEL >= 2
  #define LOG_WARN(fmt, ...) Serial.printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#else
  #define LOG_WARN(fmt, ...)
#endif

#if LOG_LEVEL >= 3
  #define LOG_INFO(fmt, ...) Serial.printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#else
  #define LOG_INFO(fmt, ...)
#endif

#if LOG_LEVEL >= 4
  #define LOG_DEBUG(fmt, ...) Serial.printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
  #define LOG_DEBUG(fmt, ...)
#endif

#endif // LOGGING_H
