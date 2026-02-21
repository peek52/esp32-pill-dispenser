---
name: pill-dispenser-module
description: A skill for developing and maintaining the ESP32 Medicine Dispenser system. Use this when adding features, fixing bugs, or modifying the pill dispensing system.
---

# ESP32 Medicine Dispenser Skill

You are an expert in ESP32 development, PlatformIO, and embedded systems for IoT medical devices. When this skill is activated, you must strictly follow these rules:

---

## ⛔ กฎเหล็ก: ห้าม delay() ทุกกรณี!

```cpp
// ❌ ห้ามใช้ delay() ทุกที่ รวมถึง setup()
delay(1000);      // ผิด!
delay(100);       // ผิด!
vTaskDelay(100);  // ผิด!

// ✅ ใช้ millis() เท่านั้น
static unsigned long startTime = millis();
if (millis() - startTime > 1000) {
  // do something
}
```

### Non-blocking แทน delay() ใน setup()

```cpp
// ❌ ผิด
void setup() {
  Serial.begin(115200);
  delay(1000);  // รอ Serial
}

// ✅ ถูก - ใช้ while loop with timeout
void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 1000) {
    // รอ Serial แต่ไม่เกิน 1 วินาที
  }
}
```

---

## 1. File Structure Standards

```
project/
├── include/
│   ├── config.h         # ค่า config ทั้งหมด (pins, timing)
│   ├── secrets.h         # Credentials (ไม่ commit)
│   ├── logging.h         # LOG macros
│   ├── netpie.h          # MQTT module header
│   ├── dispenser.h       # Servo/dispense logic header
│   ├── scheduler.h       # Time scheduling header
│   └── telegram.h        # Bot notification header
├── src/
│   ├── main.cpp          # Entry point, callbacks, loop
│   ├── netpie.cpp        # MQTT implementation
│   ├── dispenser.cpp     # Servo state machine
│   ├── scheduler.cpp     # RTC/NTP scheduling
│   └── telegram.cpp      # Telegram bot
└── .gitignore            # ต้องมี secrets.h
```

---

## 2. Offline Mode (สำคัญมาก!)

ระบบต้องทำงานได้แม้ไม่มี WiFi:

### 2.1 Offline Capabilities
| ฟีเจอร์ | Offline Mode |
|---------|-------------|
| ปุ่ม Manual Dispense | ✅ ทำงานได้ |
| Schedule จาก RTC | ✅ ทำงานได้ |
| LED Indicator | ✅ ทำงานได้ |
| IR Sensor | ✅ ทำงานได้ |
| Servo Control | ✅ ทำงานได้ |
| NETPIE | ⏸️ Queue ไว้ส่งทีหลัง |
| Telegram | ⏸️ Queue ไว้ส่งทีหลัง |

### 2.2 Offline Implementation Pattern

```cpp
// Flag สำหรับ offline mode
bool isOnline = false;

// Queue สำหรับเก็บ events ตอน offline
struct OfflineEvent {
  unsigned long timestamp;
  String type;
  String data;
};
std::vector<OfflineEvent> offlineQueue;

void addToOfflineQueue(String type, String data) {
  if (offlineQueue.size() < 50) {  // จำกัด 50 events
    offlineQueue.push_back({rtc.now().unixtime(), type, data});
  }
}

void syncOfflineQueue() {
  if (!isOnline || offlineQueue.empty()) return;
  
  for (auto& event : offlineQueue) {
    // ส่ง events ที่ค้างอยู่
    updateShadow(event.type, event.data);
  }
  offlineQueue.clear();
}
```

### 2.3 WiFi Check (Non-blocking)

```cpp
void checkConnectivity() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 30000) return;
  lastCheck = millis();
  
  isOnline = (WiFi.status() == WL_CONNECTED);
  
  if (isOnline && !offlineQueue.empty()) {
    syncOfflineQueue();
  }
}
```

---

## 3. State Machine Pattern (ใช้ทุกที่)

### 3.1 Generic State Machine

```cpp
enum class State { IDLE, WORKING, WAITING, COMPLETE };

class StateMachine {
  State state = State::IDLE;
  unsigned long timer = 0;
  
public:
  void start() {
    state = State::WORKING;
    timer = millis();
  }
  
  void update() {
    switch (state) {
      case State::IDLE: break;
      
      case State::WORKING:
        // ทำงาน
        if (workDone()) {
          state = State::WAITING;
          timer = millis();
        }
        break;
        
      case State::WAITING:
        if (millis() - timer > WAIT_TIME) {
          state = State::COMPLETE;
        }
        break;
        
      case State::COMPLETE:
        // cleanup
        state = State::IDLE;
        break;
    }
  }
};
```

### 3.2 Dispenser State Machine

```cpp
enum DispenseState { 
  DISP_IDLE, 
  DISP_OPENING, 
  DISP_DETECTING, 
  DISP_CLOSING, 
  DISP_COMPLETE 
};

static DispenseState state = DISP_IDLE;
static unsigned long stateTimer = 0;

void dispenserLoop() {
  switch (state) {
    case DISP_IDLE: break;
    
    case DISP_OPENING:
      if (millis() - stateTimer > 800) {
        state = DISP_DETECTING;
        stateTimer = millis();
      }
      break;
      
    case DISP_DETECTING:
      if (checkPillDetected() || millis() - stateTimer > 200) {
        state = DISP_CLOSING;
        stateTimer = millis();
      }
      break;
      
    case DISP_CLOSING:
      setServo(0);
      if (millis() - stateTimer > 500) {
        state = DISP_COMPLETE;
      }
      break;
      
    case DISP_COMPLETE:
      // Log & notify
      state = DISP_IDLE;
      break;
  }
}
```

---

## 4. WiFi Manager (Non-blocking)

```cpp
void setupWiFiManager() {
  LOG_INFO("[WiFi] Starting...");
  
  wifiManager.setConfigPortalTimeout(120);
  wifiManager.setConnectTimeout(30);
  
  // ไม่ใช้ delay - ถ้า fail ก็ทำงาน offline
  if (!wifiManager.autoConnect(WIFI_AP_NAME)) {
    LOG_WARN("[WiFi] Failed, running in OFFLINE mode");
    isOnline = false;
    return;  // ไม่ restart!
  }
  
  isOnline = true;
  LOG_INFO("[WiFi] Connected: %s", WiFi.localIP().toString().c_str());
}
```

---

## 5. NETPIE Integration (Offline-aware)

```cpp
void updateShadow(String status, String detail) {
  if (!isOnline || !mqttClient.connected()) {
    // เก็บไว้ใน queue
    addToOfflineQueue(status, detail);
    return;
  }
  
  static char buffer[256];
  snprintf(buffer, sizeof(buffer), 
    "{\"data\":{\"status\":\"%s\",\"detail\":\"%s\"}}",
    status.c_str(), detail.c_str());
  mqttClient.publish("@shadow/data/update", buffer);
}
```

---

## 6. Logging Standards

```cpp
LOG_DEBUG("[Module] Debug info: %d", value);
LOG_INFO("[Module] Important event: %s", name);
LOG_WARN("[Module] Warning condition");
LOG_ERROR("[Module] Error occurred: %d", errorCode);
```

**กฎ:**
- ใส่ `[ModuleName]` prefix เสมอ
- ไม่ใช้ String concatenation ใน log

---

## 7. Configuration

### config.h
```cpp
#define DEVICE_NAME "pill-dispenser"
#define FIRMWARE_VERSION "2.0.0"

// Pins
#define I2C_SDA 8
#define I2C_SCL 9
#define SWITCH_PIN 6
#define LED_PIN 2

// Timing (milliseconds)
#define LOOP_INTERVAL 100
#define WIFI_CHECK_INTERVAL 30000
#define NTP_SYNC_INTERVAL 3600000
```

### secrets.h (ไม่ commit)
```cpp
#define MQTT_CLIENT_ID "xxx"
#define MQTT_TOKEN "xxx"
#define BOT_TOKEN "xxx"
```

---

## 8. Memory Best Practices

```cpp
// ✅ ใช้ static buffer
static char buffer[128];
snprintf(buffer, sizeof(buffer), "Value: %d", value);

// ❌ ห้าม String concatenation
String msg = "Value: " + String(value);  // กิน heap!
```

---

## 9. Pre-Deploy Checklist

### Build
- [ ] `pio run` ไม่มี Error/Warning
- [ ] Flash usage < 80%
- [ ] `grep -r "delay(" src/` **ต้องไม่มีผลลัพธ์**

### Offline Mode
- [ ] ปุ่มทำงานเมื่อไม่มี WiFi
- [ ] Schedule ทำงานจาก RTC
- [ ] Reconnect อัตโนมัติ
- [ ] Queue sync เมื่อ online

### Connectivity
- [ ] WiFi reconnect (non-blocking)
- [ ] MQTT reconnect (non-blocking)
- [ ] NTP sync with retry

### Hardware
- [ ] Servo เคลื่อนที่ถูกต้อง
- [ ] IR sensor ตรวจจับได้
- [ ] LED indicator ทำงาน

---

## 10. Common Patterns

### Non-blocking Retry

```cpp
class NonBlockingRetry {
  unsigned long lastAttempt = 0;
  int attempts = 0;
  int maxAttempts;
  unsigned long interval;
  
public:
  NonBlockingRetry(int max, unsigned long ms) 
    : maxAttempts(max), interval(ms) {}
  
  bool shouldRetry() {
    if (attempts >= maxAttempts) return false;
    if (millis() - lastAttempt < interval) return false;
    lastAttempt = millis();
    attempts++;
    return true;
  }
  
  void reset() { attempts = 0; }
};
```

### Debounced Button (Non-blocking)

```cpp
class DebouncedButton {
  uint8_t pin;
  bool lastState = HIGH;
  unsigned long lastChange = 0;
  unsigned long debounceMs;
  
public:
  DebouncedButton(uint8_t p, unsigned long ms = 50) 
    : pin(p), debounceMs(ms) {}
  
  bool pressed() {
    bool current = digitalRead(pin);
    
    if (current != lastState && millis() - lastChange > debounceMs) {
      lastChange = millis();
      lastState = current;
      return (current == LOW);
    }
    return false;
  }
};
```

---

## 11. Response Language

- อธิบาย code และ decisions เป็น**ภาษาไทย**
- Comments สามารถเป็นภาษาไทยได้
- Variable/function names ใช้ภาษาอังกฤษ
