---
name: esp32-module-standard
description: A skill for creating new sensor modules and components for the ESP32 Pill Dispenser System. Use this when adding new sensors, actuators, or features to the project.
---

# ESP32 Pill Dispenser Module Skill

You are an expert in ESP32 development, PlatformIO, and embedded systems. When this skill is activated, you must strictly follow these rules:

## 1. File Structure Standards

- Every new module must have **separate header (.h) and implementation (.cpp) files**
- Header files go in `include/` directory
- Implementation files go in `src/` directory
- Use the existing naming convention: `moduleName.h` and `moduleName.cpp`

## 2. Non-Blocking Architecture

- **CRITICAL:** Never use `delay()` in the main loop - use `millis()` based timing instead
- Implement a `moduleSetup()` function for initialization
- Implement a `moduleLoop()` function that gets called from `main.cpp`
- Use state machines for complex operations that require waiting

```cpp
// ✅ Correct Non-blocking pattern
static unsigned long lastReadTime = 0;
if (millis() - lastReadTime >= READ_INTERVAL) {
    lastReadTime = millis();
    // Do sensor reading here
}

// ❌ Wrong - Never use delay()
delay(1000);
```

## 3. Logging Standards

- Use the project's logging macros: `LOG_DEBUG()`, `LOG_INFO()`, `LOG_WARN()`, `LOG_ERROR()`
- Always include function context in logs: `[ModuleName]`
- Log important state changes and errors

## 4. Configuration

- Add configurable parameters to `include/config.h`
- Add sensitive data (keys, passwords) to `include/secrets.h`
- Use `#define` with clear naming: `MODULENAME_PARAMETER_NAME`

## 5. NETPIE Integration

- If the module produces sensor data, add it to the shadow update in `netpie.cpp` (or `offline_module.cpp` if offline-aware)
- Follow the existing JSON structure format
- Use consistent key naming in lowercase with underscores

## 6. Code Quality

- Include proper header guards: `#ifndef MODULENAME_H` / `#define MODULENAME_H`
- Add descriptive comments in Thai for educational purposes
- Implement input validation and error handling
- Use `extern` for global data access across files

## 7. Production Standards

### 7.1 Non-Blocking Requirements (Critical for Production)

- **ABSOLUTELY NO `delay()` ALLOWED** - ใช้ `millis()` based timing เท่านั้น
- Use **State Machines** for multi-step operations (e.g., servo movement, IR detection)
- Implement **Async I/O patterns** - never wait synchronously for network/sensor responses
- Always **yield to other tasks** using `vTaskDelay(1)` or `yield()` in long loops (if using FreeRTOS tasks)
- Implement **timeout mechanisms** for all external communications

```cpp
// ✅ Production Non-blocking State Machine Pattern
enum DispenserState { IDLE, DISPENSING, DETECTING, ERROR };
static DispenserState state = IDLE;
static unsigned long stateTimer = 0;

void dispenserLoop() {
    switch (state) {
        case IDLE:
             // Wait for trigger
            break;
        case DISPENSING:
            if (isDispenseComplete()) {
                state = DETECTING;
                stateTimer = millis();
            } else if (millis() - stateTimer > TIMEOUT_MS) {
                state = ERROR;
            }
            break;
        case DETECTING:
            if (checkPill()) {
                state = IDLE;
            } else if (millis() - stateTimer > TIMEOUT_MS) {
                state = ERROR;
            }
            break;
        case ERROR:
            handleError();
            state = IDLE;
            break;
    }
}
```

### 7.2 Error Handling & Reliability

- Ensure code is **production-ready** with proper error handling and recovery
- Implement **Watchdog Timer (WDT)** feeding in long-running operations
- Optimize **memory usage** - prefer stack allocation over heap
- Add **graceful degradation** - if a sensor fails, system should continue operating
- Use **LOG_LEVEL** appropriately

```cpp
// ✅ Production-ready error handling
bool readIRSensor() {
    static int failCount = 0;
    // ... reading logic ...
    bool detected = digitalRead(IR_PIN) == LOW;

    if (!detected && failCount > MAX_RETRIES) {
         LOG_WARN("[Dispenser] IR Sensor potentially blocked or failed");
         return false;
    }
    return detected;
}
```

## 8. Response Language

- **Crucial:** Explain your technical decisions in **Thai language** so the user can understand the implementation
- Keep the tone professional, encouraging, and easy to understand for beginners
- Reference existing modules (like `dispenser.cpp`, `netpie.cpp`) as examples

## 9. Web Dashboard Standards (NETPIE Freeboard)

- ใช้ **CSS Variables** หรือ Theme มาตรฐานของ Widget
- JavaScript widget ต้องรองรับ **Offline Fallback**/Loading State
- ใช้ **Request data** เมื่อโหลด Widget (`#["device"].readShadow()`)
- แสดง **Loading State** ขณะรอข้อมูล

```javascript
// ✅ Correct Shadow Read
#["device"].readShadow(function(err, data) {
    if(!err) {
        updateUI(data);
    }
});
```

## 10. API/Data Design Guidelines

- Key names in JSON **must match** existing keys if related (e.g., `dose1_time`, `medicineName`)
- Use **snake_case** for new key names
- Update `netpie.cpp` to handle new keys in `onScheduleUpdate` or new callbacks

## 11. Sensor/Actuator Validation

- ตรวจสอบค่า `isnan()` หรือค่าที่ผิดปกติทุกครั้ง (เช่น Servo angle > 180)
- กำหนด **MIN/MAX ที่เป็นไปได้** (e.g., IR Sensor count 0-6)
- ใช้ **"Last Known Good Value"** เมื่ออ่านค่าผิดพลาด

## 12. Memory Best Practices

- ใช้ `static` สำหรับ buffer ที่ใช้ซ้ำ
- ใช้ `strlcpy()` แทน `strcpy()`
- ตรวจสอบ `ESP.getFreeHeap()` หากมีการใช้ Dynamic Memory เยอะ
- หลีกเลี่ยง `String` concatenation แบบซ้ำๆ (ใช้ `snprintf()`)

```cpp
// ✅ Safe string handling
static char buffer[128];
snprintf(buffer, sizeof(buffer), "Status: %s, Pill: %s", status, pillName);
```

## 13. Pre-Deploy Checklist

ก่อน Deploy ต้องตรวจสอบทุกข้อ:

### Build & Compile

- [ ] `pio run` สำเร็จไม่มี Error
- [ ] ขนาด Flash ไม่เกิน 80%

### Connectivity

- [ ] WiFi Reconnect ทำงานปกติ
- [ ] NETPIE/MQTT Reconnect ทำงานปกติ
- [ ] Offline Queue ทำงานเมื่อเน็ตหลุด

### Reliability

- [ ] Watchdog Reset ทำงาน
- [ ] Memory Leak check (รันยาวๆ แล้ว Heap ไม่ลด)

### Features

- [ ] Module ใหม่ทำงานถูกต้อง
- [ ] ไม่กระทบฟีเจอร์เดิม (Regression Test)

## 14. Code Consistency Verification (สำคัญมาก!)

ก่อนสร้างหรือแก้ไข Module ใหม่ ต้องตรวจสอบความสอดคล้องของโค้ดทั้งระบบ:

### 14.1 Constants & Defines Consistency

ต้องใช้ค่าที่กำหนดใน `config.h` เท่านั้น **ห้าม hardcode ค่าซ้ำ**:

```cpp
// ❌ ผิด - Hardcode Pin
pinMode(6, INPUT);

// ✅ ถูก - ใช้ค่าจาก config.h
pinMode(SWITCH_PIN, INPUT);
```

### 14.2 ก่อนเพิ่มโค้ดใหม่ ต้องค้นหาก่อน

เมื่อจะใช้ค่า Constant หรือเรียกใช้ Library function ใหม่ **ต้อง grep ค้นหาก่อน**:

```bash
# ตัวอย่าง
grep -r "SWITCH_PIN" include/ src/
```

### 14.3 Checklist ก่อน Commit โค้ดใหม่

- [ ] ค้นหา `#define` หรือ `const` ที่เกี่ยวข้องก่อนใช้ค่าใหม่
- [ ] ไม่มีการ hardcode ค่าที่ควรจะใช้จาก `config.h`
- [ ] ตรวจสอบว่า MQTT topics ตรงกันทั้ง Publisher และ Subscriber
- [ ] ตรวจสอบว่า JSON keys ตรงกันทั้ง Sender และ Receiver
