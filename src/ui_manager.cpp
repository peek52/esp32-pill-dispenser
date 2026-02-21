#include "ui_manager.h"
#include "config.h"
#include "display_module.h"
#include "scheduler.h"
#include "servo_control.h"
#include "wifi_manager.h"
#include <WiFi.h>

// ============================================================
// Color Palette — Light Mint "Production" Theme
// ============================================================
#define COL_BG       0xF7DE // Very light mint background
#define COL_CARD     0xFFFF // Clean white for cards
#define COL_PRIMARY  0x2652 // Teal/Mint (#20C997)
#define COL_ACCENT   0x15D0 // Darker teal (#12B886)
#define COL_SUCCESS  0x460A // Green (#40C057)
#define COL_DANGER   0xFA8A // Red (#FA5252)
#define COL_WARN     0xFC62 // Orange
#define COL_TEXT     0x31C8 // Main texts (#343A40)
#define COL_TEXT_INV 0xFFFF // White text (for buttons/headers)
#define COL_TEXT_DIM 0x8472 // Light gray for secondary text (#868E96)
#define COL_BTN      0xEF7D // Default button bg (#E9ECEF)
#define COL_BTN_ON   0x2652 // Active toggle
#define COL_DIVIDER  0xDF1C // Subtle divider lines (#DEE2E6)
#define COL_SHADOW   0xCE59 // Button shadow (#CED4DA)

// ============================================================
// ============================================================
// Slot labels (English short)
// ============================================================
static const char *slotShort[NUM_TIME_SLOTS] = {"M.Bf", "M.Af", "N.Bf", "N.Af",
                                                "E.Bf", "E.Af", "Bed"};
static const char *periodName[] = {"Morning", "Noon", "Evening", "Bedtime"};

// Preset medicine names
static const char *presetNames[] = {
    "Slot 1",  "Slot 2",      "Slot 3",    "Slot 4",    "Slot 5",
    "Slot 6",  "Paracetamol", "Vitamin C", "Antacid",   "Cough Med",
    "Allergy", "Antibiotic",  "Ibuprofen", "Omeprazole"};
static const int numPresets = sizeof(presetNames) / sizeof(presetNames[0]);

// ============================================================
// State
// ============================================================
static Screen currentScreen = SCREEN_HOME;
static unsigned long lastClockTick = 0;
static unsigned long lastTouchMs = 0;
#define DEBOUNCE 300

static int editSlotIdx = -1;
static int editModIdx = -1;
static uint8_t editH = 8, editM = 0;
static int modPage = 0;

static LGFX_Sprite canvas;

static ManualDispenseCallback manualCb = nullptr;
void uiSetManualDispenseCallback(ManualDispenseCallback cb) { manualCb = cb; }

static ConfirmDispenseCallback confirmCb = nullptr;
static int confirmSlotIdx = -1;
static unsigned long confirmStartMs = 0;

// ============================================================
// WiFi UI State
// ============================================================
static int wifiScanCount = 0;
static int wifiScanPage = 0;
static String wifiNetworks[20];
static bool wifiScanning = false;
static bool wifiNeedsScan = false;
static String selectedSSID = "";
static String inputPassword = "";
static bool oskShift = false;
static bool portalActive = false;

void uiSetConfirmDispenseCallback(ConfirmDispenseCallback cb) {
  confirmCb = cb;
}

// ============================================================
// Forward declarations
// ============================================================
static void drawHome();
static void drawSchedule();
static void drawTimePicker();
static void drawModules();
static void drawModuleDetail();
static void drawManualDispense();
static void drawConfirmDispense();
static void drawWifiMenu();
static void drawWifiOSK();
static void drawWifiPortal();
static void drawWifiScan();
static void touchHome(int x, int y);
static void touchSchedule(int x, int y);
static void touchTimePicker(int x, int y);
static void touchModules(int x, int y);
static void touchModuleDetail(int x, int y);
static void touchManualDispense(int x, int y);
static void touchConfirmDispense(int x, int y);
static void touchWifiMenu(int x, int y);
static void touchWifiOSK(int x, int y);
static void touchWifiPortal(int x, int y);
static void touchWifiScan(int x, int y);
static void switchTo(Screen s);
static void btn(int x, int y, int w, int h, const char *txt, uint16_t bg,
                uint16_t fg);

void uiShowConfirmDispense(int timeSlotIndex) {
  confirmSlotIdx = timeSlotIndex;
  confirmStartMs = millis();
  switchTo(SCREEN_CONFIRM_DISPENSE);
}

// ============================================================
void uiSetup() {
  canvas.setPsram(true);
  canvas.setColorDepth(16);
  canvas.createSprite(480, 320);
  switchTo(SCREEN_HOME);
}

// ============================================================
// Main loop
// ============================================================
void uiLoop() {
  LGFX &lcd = getDisplay();

  // Live clock on home screen
  if (currentScreen == SCREEN_HOME && millis() - lastClockTick >= 1000) {
    lastClockTick = millis();
    uint8_t h, m, s;
    schedulerGetTime(h, m, s);

    // Big time
    char buf[9];
    sprintf(buf, "%02d:%02d:%02d", h, m, s);
    canvas.setFont(&fonts::FreeSansBold24pt7b);
    canvas.setTextDatum(middle_center);
    canvas.setTextColor(COL_TEXT, COL_BG);
    canvas.fillRect(30, 68, 300, 55, COL_BG);
    canvas.drawString(buf, 175, 95);

    // Next schedule
    canvas.setFont(&fonts::FreeSans12pt7b);
    canvas.fillRect(30, 153, 300, 35, COL_BG);
    canvas.setTextDatum(middle_center);
    int next = schedulerNextSlot();
    if (next >= 0) {
      TimeSlot &ts = timeSlotGet(next);
      char nb[40];
      sprintf(nb, "Next: %s  %02d:%02d", slotShort[next], ts.hour, ts.minute);
      canvas.setTextColor(COL_SUCCESS, COL_BG);
      canvas.drawString(nb, 175, 172);
    } else {
      canvas.setTextColor(COL_TEXT_DIM, COL_BG);
      canvas.drawString("No upcoming schedule", 175, 172);
    }
    canvas.pushSprite(&getDisplay(), 0, 0);
  }

  // Handle Captive Portal blocking wait
  if (currentScreen == SCREEN_WIFI_PORTAL && !portalActive) {
    portalActive = true;
    canvas.pushSprite(&getDisplay(), 0, 0); // Show "look at phone" text
    wifiStartPortal();
    portalActive = false;
    switchTo(SCREEN_HOME);
  }

  // Handle WiFi Scan
  if (currentScreen == SCREEN_WIFI_SCAN && wifiNeedsScan) {
    wifiNeedsScan = false;
    wifiScanning = true;
    drawWifiScan();
    canvas.pushSprite(&getDisplay(), 0, 0);

    wifiScanCount = WiFi.scanNetworks();
    if (wifiScanCount > 20)
      wifiScanCount = 20;
    for (int i = 0; i < wifiScanCount; i++) {
      wifiNetworks[i] = WiFi.SSID(i);
    }
    wifiScanning = false;
    wifiScanPage = 0;
    switchTo(SCREEN_WIFI_SCAN); // Redraw list
  }

  // Handle Captive Portal blocking wait
  if (currentScreen == SCREEN_WIFI_PORTAL && !portalActive) {
    portalActive = true;
    canvas.pushSprite(&getDisplay(), 0, 0); // Show "look at phone" text
    wifiStartPortal();
    portalActive = false;
    switchTo(SCREEN_HOME);
  }

  // Handle WiFi Scan
  if (currentScreen == SCREEN_WIFI_SCAN && wifiNeedsScan) {
    wifiNeedsScan = false;
    wifiScanning = true;
    drawWifiScan();
    canvas.pushSprite(&getDisplay(), 0, 0);

    wifiScanCount = WiFi.scanNetworks();
    if (wifiScanCount > 20)
      wifiScanCount = 20;
    for (int i = 0; i < wifiScanCount; i++) {
      wifiNetworks[i] = WiFi.SSID(i);
    }
    wifiScanning = false;
    wifiScanPage = 0;
    switchTo(SCREEN_WIFI_SCAN); // Redraw list
  }

  // Handle countdown on confirm screen
  if (currentScreen == SCREEN_CONFIRM_DISPENSE) {
    if (millis() - confirmStartMs > 10 * 60 * 1000UL) {
      Serial.println("[UI] Dispense confirmation timed out");
      switchTo(SCREEN_HOME);
    } else if (millis() - lastClockTick >= 1000) {
      lastClockTick = millis();
      switchTo(SCREEN_CONFIRM_DISPENSE); // Redraw countdown
    }
  }

  // Touch
  lgfx::touch_point_t tp;
  if (lcd.getTouch(&tp)) {
    if (millis() - lastTouchMs < DEBOUNCE)
      return;
    lastTouchMs = millis();
    Serial.printf("[Touch] x=%d y=%d screen=%d\n", tp.x, tp.y, currentScreen);
    switch (currentScreen) {
    case SCREEN_HOME:
      touchHome(tp.x, tp.y);
      break;
    case SCREEN_SCHEDULE:
      touchSchedule(tp.x, tp.y);
      break;
    case SCREEN_TIME_PICKER:
      touchTimePicker(tp.x, tp.y);
      break;
    case SCREEN_MODULES:
      touchModules(tp.x, tp.y);
      break;
    case SCREEN_MODULE_DETAIL:
      touchModuleDetail(tp.x, tp.y);
      break;
    case SCREEN_MANUAL_DISPENSE:
      touchManualDispense(tp.x, tp.y);
      break;
    case SCREEN_CONFIRM_DISPENSE:
      touchConfirmDispense(tp.x, tp.y);
      break;
    case SCREEN_WIFI_MENU:
      touchWifiMenu(tp.x, tp.y);
      break;
    case SCREEN_WIFI_SCAN:
      touchWifiScan(tp.x, tp.y);
      break;
    case SCREEN_WIFI_OSK:
      touchWifiOSK(tp.x, tp.y);
      break;
    case SCREEN_WIFI_PORTAL:
      touchWifiPortal(tp.x, tp.y);
      break;
    default:
      break;
    }
  }
}

// ============================================================
static void switchTo(Screen s) {
  currentScreen = s;
  canvas.fillScreen(COL_BG);
  switch (s) {
  case SCREEN_HOME:
    drawHome();
    break;
  case SCREEN_SCHEDULE:
    drawSchedule();
    break;
  case SCREEN_TIME_PICKER:
    drawTimePicker();
    break;
  case SCREEN_MODULES:
    drawModules();
    break;
  case SCREEN_MODULE_DETAIL:
    drawModuleDetail();
    break;
  case SCREEN_MANUAL_DISPENSE:
    drawManualDispense();
    break;
  case SCREEN_CONFIRM_DISPENSE:
    drawConfirmDispense();
    break;
  case SCREEN_WIFI_MENU:
    drawWifiMenu();
    break;
  case SCREEN_WIFI_SCAN:
    drawWifiScan();
    break;
  case SCREEN_WIFI_OSK:
    drawWifiOSK();
    break;
  case SCREEN_WIFI_PORTAL:
    drawWifiPortal();
    break;
  default:
    break;
  }
  canvas.pushSprite(&getDisplay(), 0, 0);
}

// ============================================================
// Button helper
// ============================================================
static void btn(int x, int y, int w, int h, const char *txt, uint16_t bg,
                uint16_t fg) {
  auto &lcd = canvas;
  // Button shadow
  if (bg != COL_BG && bg != COL_CARD) {
    lcd.fillRoundRect(x, y + 2, w, h, 6, COL_SHADOW);
  }
  lcd.fillRoundRect(x, y, w, h, 6, bg);
  if (bg == COL_BTN) {
    lcd.drawRoundRect(x, y, w, h, 6, COL_DIVIDER);
  }
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(fg, bg);
  lcd.drawString(txt, x + w / 2, y + h / 2 - 1);
}

static void drawShadowCard(int x, int y, int w, int h) {
  canvas.fillRoundRect(x, y+3, w, h, 8, COL_SHADOW);
  canvas.fillRoundRect(x, y, w, h, 8, COL_CARD);
  // canvas.drawRoundRect(x, y, w, h, 8, COL_DIVIDER); // Optional subtle border
}

// ============================================================
// HOME SCREEN
// Layout: header(0-44), left-half(clock+info), right-half(3 buttons)
// ============================================================
static void drawHome() {
  auto &lcd = canvas;

  // Header bar
  lcd.fillRect(0, 0, 480, 44, COL_PRIMARY);
  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_TEXT_INV, COL_PRIMARY);
  lcd.drawString("Medicine Dispenser", 240, 22);

  // --- Left side: Clock ---
  uint8_t h, m, s;
  schedulerGetTime(h, m, s);
  char timeBuf[9];
  sprintf(timeBuf, "%02d:%02d:%02d", h, m, s);
  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_TEXT, COL_BG);
  lcd.drawString(timeBuf, 175, 95);

  // Date
  uint16_t yr;
  uint8_t mo, dy, dow;
  schedulerGetDate(yr, mo, dy, dow);
  const char *dn[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  char dateBuf[24];
  sprintf(dateBuf, "%s %02d/%02d/%04d", dn[dow], dy, mo, yr);
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextColor(COL_TEXT_DIM, COL_BG);
  lcd.setTextDatum(middle_center);
  lcd.drawString(dateBuf, 175, 135);

  // Next schedule
  int next = schedulerNextSlot();
  lcd.setFont(&fonts::FreeSans12pt7b);
  if (next >= 0) {
    TimeSlot &ts = timeSlotGet(next);
    char nb[40];
    sprintf(nb, "Next: %s  %02d:%02d", slotShort[next], ts.hour, ts.minute);
    lcd.setTextColor(COL_SUCCESS, COL_BG);
    lcd.drawString(nb, 175, 172);
  } else {
    lcd.setTextColor(COL_TEXT_DIM, COL_BG);
    lcd.drawString("No upcoming schedule", 175, 172);
  }

  // Status indicator
  bool on = schedulerIsEnabled();
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextColor(on ? COL_SUCCESS : COL_DANGER, COL_BG);
  lcd.drawString(on ? "Schedule: ON" : "Schedule: OFF", 175, 197);

  // Vertical divider
  lcd.drawFastVLine(345, 50, 220, COL_DIVIDER);

  // --- Right side: 3 buttons + toggle ---
  btn(360, 55, 110, 50, "Schedule", COL_PRIMARY, COL_TEXT_INV);
  btn(360, 115, 110, 50, "Modules", COL_ACCENT, COL_TEXT_INV);
  btn(360, 175, 110, 50, "Dispense", COL_DANGER, COL_TEXT);

  // Bottom toggle
  lcd.drawFastHLine(0, 270, 480, COL_DIVIDER);
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextDatum(middle_left);
  lcd.setTextColor(COL_TEXT_DIM, COL_BG);
  lcd.drawString("Auto:", 15, 295);
  btn(65, 280, 80, 30, on ? "ON" : "OFF", on ? COL_SUCCESS : COL_BTN, COL_TEXT);

  // WiFi Button
  bool wifiOk = wifiIsConnected();
  btn(160, 280, 180, 30, wifiOk ? wifiGetSSID().c_str() : "WiFi: Disconnected",
      wifiOk ? COL_SUCCESS : COL_BTN, wifiOk ? COL_BG : COL_DANGER);
}

// ============================================================
// SCHEDULE SCREEN — 2x2 grid (4 periods)
// ============================================================
static void drawSchedule() {
  auto &lcd = canvas;

  // Header
  lcd.fillRect(0, 0, 480, 40, COL_PRIMARY);
  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_TEXT_INV, COL_PRIMARY);
  lcd.drawString("Schedule", 240, 20);
  btn(5, 5, 60, 30, "Back", COL_CARD, COL_TEXT);

  // 2x2 grid
  int cw = 228, ch = 125;
  int startX = 5, startY = 46;

  for (int p = 0; p < 4; p++) {
    int col = p % 2, row = p / 2;
    int cx = startX + col * 237;
    int cy = startY + row * 132;

    // Card background
    drawShadowCard(cx, cy, cw, ch);

    // Period label
    lcd.setFont(&fonts::FreeSans9pt7b);
    lcd.setTextDatum(top_left);
    lcd.setTextColor(COL_PRIMARY, COL_CARD);
    lcd.drawString(periodName[p], cx + 12, cy + 8);

    // Divider inside card
    lcd.drawFastHLine(cx + 8, cy + 28, cw - 16, COL_DIVIDER);

    lcd.setFont(&fonts::FreeSans9pt7b);

    if (p < 3) {
      int s1 = p * 2, s2 = p * 2 + 1;
      TimeSlot &t1 = timeSlotGet(s1);
      TimeSlot &t2 = timeSlotGet(s2);

      // Before meal
      lcd.setTextDatum(middle_left);
      lcd.setTextColor(COL_TEXT_DIM, COL_CARD);
      lcd.drawString("Before", cx + 12, cy + 48);
      char b1[8];
      sprintf(b1, "%02d:%02d", t1.hour, t1.minute);
      btn(cx + 100, cy + 35, 80, 28, b1, t1.enabled ? COL_PRIMARY : COL_BTN,
          COL_TEXT);
      btn(cx + 188, cy + 35, 32, 28, t1.enabled ? "ON" : "--",
          t1.enabled ? COL_SUCCESS : COL_BTN, COL_TEXT);

      // After meal
      lcd.setTextDatum(middle_left);
      lcd.setTextColor(COL_TEXT_DIM, COL_CARD);
      lcd.drawString("After", cx + 12, cy + 88);
      char b2[8];
      sprintf(b2, "%02d:%02d", t2.hour, t2.minute);
      btn(cx + 100, cy + 75, 80, 28, b2, t2.enabled ? COL_PRIMARY : COL_BTN,
          COL_TEXT);
      btn(cx + 188, cy + 75, 32, 28, t2.enabled ? "ON" : "--",
          t2.enabled ? COL_SUCCESS : COL_BTN, COL_TEXT);
    } else {
      // Bedtime — single row centered
      TimeSlot &t6 = timeSlotGet(6);
      lcd.setTextDatum(middle_left);
      lcd.setTextColor(COL_TEXT_DIM, COL_CARD);
      lcd.drawString("Time", cx + 12, cy + 65);
      char b6[8];
      sprintf(b6, "%02d:%02d", t6.hour, t6.minute);
      btn(cx + 100, cy + 52, 80, 28, b6, t6.enabled ? COL_PRIMARY : COL_BTN,
          COL_TEXT);
      btn(cx + 188, cy + 52, 32, 28, t6.enabled ? "ON" : "--",
          t6.enabled ? COL_SUCCESS : COL_BTN, COL_TEXT);
    }
  }
}

// ============================================================
// TIME PICKER
// ============================================================
static void drawTimePicker() {
  auto &lcd = canvas;

  // Header
  lcd.fillRect(0, 0, 480, 40, COL_PRIMARY);
  char title[32];
  sprintf(title, "Set Time - %s", slotShort[editSlotIdx]);
  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_TEXT_INV, COL_PRIMARY);
  lcd.drawString(title, 240, 20);

  // Card background
  lcd.fillRoundRect(80, 60, 320, 140, 10, COL_CARD);

  // Labels
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_TEXT_DIM, COL_CARD);
  lcd.drawString("Hour", 180, 80);
  lcd.drawString("Min", 300, 80);

  // Hour +/-
  btn(150, 90, 60, 35, "+", COL_PRIMARY, COL_TEXT_INV);
  btn(150, 165, 60, 35, "-", COL_PRIMARY, COL_TEXT_INV);

  // Minute +/-
  btn(270, 90, 60, 35, "+", COL_PRIMARY, COL_TEXT_INV);
  btn(270, 165, 60, 35, "-", COL_PRIMARY, COL_TEXT_INV);

  // Time display
  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_TEXT, COL_CARD);
  char hBuf[4];
  sprintf(hBuf, "%02d", editH);
  char mBuf[4];
  sprintf(mBuf, "%02d", editM);
  lcd.drawString(hBuf, 180, 145);
  lcd.setTextColor(COL_PRIMARY, COL_CARD);
  lcd.drawString(":", 240, 140);
  lcd.setTextColor(COL_TEXT, COL_CARD);
  lcd.drawString(mBuf, 300, 145);

  // Save / Cancel
  btn(100, 230, 140, 45, "Save", COL_SUCCESS, COL_TEXT);
  btn(260, 230, 140, 45, "Cancel", COL_BTN, COL_TEXT);
}

// ============================================================
// MODULES LIST — 3 per page
// ============================================================
static void drawModules() {
  auto &lcd = canvas;

  // Header
  lcd.fillRect(0, 0, 480, 40, COL_ACCENT);
  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_TEXT_INV, COL_ACCENT);
  lcd.drawString("Medicine Modules", 240, 20);
  btn(5, 5, 60, 30, "Back", COL_CARD, COL_TEXT);

  // Page
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextColor(COL_TEXT_INV, COL_ACCENT);
  lcd.setTextDatum(middle_right);
  lcd.drawString(modPage == 0 ? "1/2" : "2/2", 470, 20);

  // 3 cards
  for (int i = 0; i < 3; i++) {
    int idx = modPage + i;
    if (idx >= NUM_MODULES)
      break;
    MedModule &mod = moduleGet(idx);
    int cy = 48 + i * 84;

    // Card
    drawShadowCard(10, cy, 460, 76);

    // Module number badge
    lcd.fillRoundRect(18, cy + 6, 30, 24, 4, COL_PRIMARY);
    lcd.setFont(&fonts::FreeSans9pt7b);
    lcd.setTextDatum(middle_center);
    lcd.setTextColor(COL_TEXT_INV, COL_PRIMARY);
    char num[3];
    sprintf(num, "%d", idx + 1);
    lcd.drawString(num, 33, cy + 18);

    // Name
    lcd.setFont(&fonts::FreeSans9pt7b);
    lcd.setTextDatum(middle_left);
    lcd.setTextColor(COL_TEXT, COL_CARD);
    lcd.drawString(mod.name, 56, cy + 18);

    // Qty (right side)
    char qBuf[10];
    sprintf(qBuf, "x%d", mod.qty);
    lcd.setFont(&fonts::FreeSansBold12pt7b);
    lcd.setTextDatum(middle_right);
    lcd.setTextColor(COL_ACCENT, COL_CARD);
    lcd.drawString(qBuf, 458, cy + 18);

    // Slot chips row
    lcd.setFont(&fonts::Font2);
    for (int s = 0; s < NUM_TIME_SLOTS; s++) {
      bool on = mod.slotMask & (1 << s);
      int px = 18 + s * 62;
      int py = cy + 46;
      lcd.fillRoundRect(px, py, 56, 22, 4, on ? COL_ACCENT : COL_BTN);
      lcd.setTextDatum(middle_center);
      lcd.setTextColor(on ? COL_BG : COL_TEXT_DIM, on ? COL_ACCENT : COL_BTN);
      lcd.drawString(slotShort[s], px + 28, py + 11);
    }
  }

  // Nav buttons
  if (modPage > 0)
    btn(10, 300, 90, 18, "< Prev", COL_BTN, COL_TEXT);
  if (modPage + 3 < NUM_MODULES)
    btn(380, 300, 90, 18, "Next >", COL_BTN, COL_TEXT);
}

// ============================================================
// MODULE DETAIL — edit name, qty, slot toggles
// ============================================================
static void drawModuleDetail() {
  auto &lcd = canvas;
  MedModule &mod = moduleGet(editModIdx);

  // Header
  lcd.fillRect(0, 0, 480, 40, COL_ACCENT);
  char title[20];
  sprintf(title, "Module %d", editModIdx + 1);
  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_TEXT_INV, COL_ACCENT);
  lcd.drawString(title, 240, 20);
  btn(5, 5, 60, 30, "Back", COL_CARD, COL_TEXT);

  // --- Name ---
  drawShadowCard(15, 50, 450, 50);
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextDatum(middle_left);
  lcd.setTextColor(COL_TEXT_DIM, COL_CARD);
  lcd.drawString("Name:", 25, 75);
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextColor(COL_TEXT, COL_CARD);
  lcd.setTextDatum(middle_center);
  lcd.drawString(mod.name, 240, 75);
  btn(380, 58, 75, 34, "Change", COL_PRIMARY, COL_TEXT_INV);

  // --- Qty ---
  drawShadowCard(15, 110, 450, 55);
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextDatum(middle_left);
  lcd.setTextColor(COL_TEXT_DIM, COL_CARD);
  lcd.drawString("Qty:", 25, 138);
  btn(180, 118, 60, 38, "-", COL_PRIMARY, COL_TEXT_INV);
  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_TEXT, COL_CARD);
  char qBuf[6];
  sprintf(qBuf, "%d", mod.qty);
  lcd.drawString(qBuf, 290, 138);
  btn(360, 118, 60, 38, "+", COL_PRIMARY, COL_TEXT_INV);

  // --- Slot toggles ---
  drawShadowCard(15, 175, 450, 85);
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextDatum(top_left);
  lcd.setTextColor(COL_TEXT_DIM, COL_CARD);
  lcd.drawString("Dispense at:", 25, 182);

  // 7 chips: row 1 = 4, row 2 = 3
  for (int s = 0; s < NUM_TIME_SLOTS; s++) {
    bool on = mod.slotMask & (1 << s);
    int col = s % 4;
    int row = s / 4;
    int px = 22 + col * 112;
    int py = 204 + row * 30;
    btn(px, py, 104, 26, slotShort[s], on ? COL_ACCENT : COL_BTN,
        on ? COL_BG : COL_TEXT_DIM);
  }

  // Save
  btn(140, 272, 200, 42, "Save", COL_SUCCESS, COL_TEXT);
}

// ============================================================
// DISPENSING / RESULT
// ============================================================
void uiShowDispensing(int moduleIndex) {
  LGFX &lcd = getDisplay();
  lcd.fillScreen(COL_BG);

  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_PRIMARY, COL_BG);
  lcd.drawString("Dispensing...", 240, 100);

  MedModule &mod = moduleGet(moduleIndex);
  char buf[32];
  sprintf(buf, "Module %d: %s", moduleIndex + 1, mod.name);
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextColor(COL_TEXT_DIM, COL_BG);
  lcd.drawString(buf, 240, 140);

  // Simple loading animation
  for (int f = 0; f < 8; f++) {
    lcd.fillCircle(200 + f * 12, 200, 5, COL_PRIMARY);
    delay(100);
  }
  delay(200);
}

void uiShowResult(int timeSlotIndex, bool success) {
  LGFX &lcd = getDisplay();
  lcd.fillScreen(COL_BG);

  uint16_t col = success ? COL_SUCCESS : COL_DANGER;

  // Big circle
  lcd.fillCircle(240, 110, 50, col);
  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_TEXT_INV, col);
  lcd.drawString(success ? "OK" : "X", 240, 110);

  // Message
  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextColor(col, COL_BG);
  lcd.drawString(success ? "Dispensed!" : "Error!", 240, 200);

  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextColor(COL_TEXT_DIM, COL_BG);
  lcd.drawString("Returning home...", 240, 250);

  delay(3000);
  switchTo(SCREEN_HOME);
}

// ============================================================
// TOUCH: HOME
// ============================================================
static void touchHome(int x, int y) {
  // Schedule: 360-470, 55-105
  if (x >= 360 && x <= 470 && y >= 55 && y <= 105) {
    Serial.println("[UI] -> Schedule");
    switchTo(SCREEN_SCHEDULE);
    return;
  }
  // Modules: 360-470, 115-165
  if (x >= 360 && x <= 470 && y >= 115 && y <= 165) {
    Serial.println("[UI] -> Modules");
    modPage = 0;
    switchTo(SCREEN_MODULES);
    return;
  }
  // Dispense: 360-470, 175-225
  if (x >= 360 && x <= 470 && y >= 175 && y <= 225) {
    Serial.println("[UI] -> Manual Dispense Screen");
    switchTo(SCREEN_MANUAL_DISPENSE);
    return;
  }
  // Toggle: 65-145, 280-310
  if (x >= 65 && x <= 145 && y >= 280 && y <= 310) {
    Serial.println("[UI] Toggle schedule");
    schedulerSetEnabled(!schedulerIsEnabled());
    schedulerSave();
    switchTo(SCREEN_HOME);
    return;
  }
  // WiFi: 160-340, 280-310
  if (x >= 160 && x <= 340 && y >= 280 && y <= 310) {
    Serial.println("[UI] -> WiFi Menu");
    switchTo(SCREEN_WIFI_MENU);
    return;
  }
}

// ============================================================
// TOUCH: SCHEDULE
// ============================================================
static void touchSchedule(int x, int y) {
  // Back: 5-65, 5-35
  if (x <= 65 && y <= 40) {
    schedulerSave();
    switchTo(SCREEN_HOME);
    return;
  }

  int startX = 5, startY = 46;

  for (int p = 0; p < 4; p++) {
    int c = p % 2, r = p / 2;
    int cx = startX + c * 237;
    int cy = startY + r * 132;

    // Skip if touch not in this card
    if (x < cx || x > cx + 228 || y < cy || y > cy + 125)
      continue;

    Serial.printf("[UI] Schedule card %d touched\n", p);

    if (p < 3) {
      int s1 = p * 2, s2 = p * 2 + 1;

      // Before time: cx+100..cx+180, cy+35..cy+63
      if (x >= cx + 100 && x <= cx + 180 && y >= cy + 35 && y <= cy + 63) {
        editSlotIdx = s1;
        editH = timeSlotGet(s1).hour;
        editM = timeSlotGet(s1).minute;
        switchTo(SCREEN_TIME_PICKER);
        return;
      }
      // Before toggle: cx+188..cx+220, cy+35..cy+63
      if (x >= cx + 188 && x <= cx + 220 && y >= cy + 35 && y <= cy + 63) {
        timeSlotGet(s1).enabled = !timeSlotGet(s1).enabled;
        switchTo(SCREEN_SCHEDULE);
        return;
      }
      // After time: cx+100..cx+180, cy+75..cy+103
      if (x >= cx + 100 && x <= cx + 180 && y >= cy + 75 && y <= cy + 103) {
        editSlotIdx = s2;
        editH = timeSlotGet(s2).hour;
        editM = timeSlotGet(s2).minute;
        switchTo(SCREEN_TIME_PICKER);
        return;
      }
      // After toggle: cx+188..cx+220, cy+75..cy+103
      if (x >= cx + 188 && x <= cx + 220 && y >= cy + 75 && y <= cy + 103) {
        timeSlotGet(s2).enabled = !timeSlotGet(s2).enabled;
        switchTo(SCREEN_SCHEDULE);
        return;
      }
    } else {
      // Bedtime time: cx+100..cx+180, cy+52..cy+80
      if (x >= cx + 100 && x <= cx + 180 && y >= cy + 52 && y <= cy + 80) {
        editSlotIdx = 6;
        editH = timeSlotGet(6).hour;
        editM = timeSlotGet(6).minute;
        switchTo(SCREEN_TIME_PICKER);
        return;
      }
      // Bedtime toggle: cx+188..cx+220, cy+52..cy+80
      if (x >= cx + 188 && x <= cx + 220 && y >= cy + 52 && y <= cy + 80) {
        timeSlotGet(6).enabled = !timeSlotGet(6).enabled;
        switchTo(SCREEN_SCHEDULE);
        return;
      }
    }
  }
}

// ============================================================
// TOUCH: TIME PICKER
// ============================================================
static void touchTimePicker(int x, int y) {
  // H+: 150-210, 90-125
  if (x >= 150 && x <= 210 && y >= 90 && y <= 125) {
    editH = (editH + 1) % 24;
    switchTo(SCREEN_TIME_PICKER);
    return;
  }
  // H-: 150-210, 165-200
  if (x >= 150 && x <= 210 && y >= 165 && y <= 200) {
    editH = (editH + 23) % 24;
    switchTo(SCREEN_TIME_PICKER);
    return;
  }
  // M+: 270-330, 90-125
  if (x >= 270 && x <= 330 && y >= 90 && y <= 125) {
    editM = (editM + 5) % 60;
    switchTo(SCREEN_TIME_PICKER);
    return;
  }
  // M-: 270-330, 165-200
  if (x >= 270 && x <= 330 && y >= 165 && y <= 200) {
    editM = (editM + 55) % 60;
    switchTo(SCREEN_TIME_PICKER);
    return;
  }
  // Save: 100-240, 230-275
  if (x >= 100 && x <= 240 && y >= 230 && y <= 275) {
    timeSlotSet(editSlotIdx, editH, editM, timeSlotGet(editSlotIdx).enabled);
    schedulerSave();
    switchTo(SCREEN_SCHEDULE);
    return;
  }
  // Cancel: 260-400, 230-275
  if (x >= 260 && x <= 400 && y >= 230 && y <= 275) {
    switchTo(SCREEN_SCHEDULE);
    return;
  }
}

// ============================================================
// TOUCH: MODULES
// ============================================================
static void touchModules(int x, int y) {
  // Back: 5-65, 5-35
  if (x <= 65 && y <= 40) {
    switchTo(SCREEN_HOME);
    return;
  }
  // Prev
  if (x >= 10 && x <= 100 && y >= 295 && modPage > 0) {
    modPage -= 3;
    switchTo(SCREEN_MODULES);
    return;
  }
  // Next
  if (x >= 380 && x <= 470 && y >= 295 && modPage + 3 < NUM_MODULES) {
    modPage += 3;
    switchTo(SCREEN_MODULES);
    return;
  }
  // Card tap → detail
  for (int i = 0; i < 3; i++) {
    int idx = modPage + i;
    if (idx >= NUM_MODULES)
      break;
    int cy = 48 + i * 84;
    if (y >= cy && y <= cy + 76 && x >= 10 && x <= 470) {
      Serial.printf("[UI] Module %d tapped\n", idx);
      editModIdx = idx;
      switchTo(SCREEN_MODULE_DETAIL);
      return;
    }
  }
}

// ============================================================
// TOUCH: MODULE DETAIL
// ============================================================
static void touchModuleDetail(int x, int y) {
  MedModule &mod = moduleGet(editModIdx);

  // Back: 5-65, 5-35
  if (x <= 65 && y <= 40) {
    switchTo(SCREEN_MODULES);
    return;
  }

  // Change name: 380-455, 58-92
  if (x >= 380 && x <= 455 && y >= 58 && y <= 92) {
    int cur = 0;
    for (int i = 0; i < numPresets; i++) {
      if (strcmp(mod.name, presetNames[i]) == 0) {
        cur = i;
        break;
      }
    }
    cur = (cur + 1) % numPresets;
    moduleSetName(editModIdx, presetNames[cur]);
    switchTo(SCREEN_MODULE_DETAIL);
    return;
  }

  // Qty-: 180-240, 118-156
  if (x >= 180 && x <= 240 && y >= 118 && y <= 156) {
    if (mod.qty > 0)
      mod.qty--;
    switchTo(SCREEN_MODULE_DETAIL);
    return;
  }
  // Qty+: 360-420, 118-156
  if (x >= 360 && x <= 420 && y >= 118 && y <= 156) {
    if (mod.qty < 99)
      mod.qty++;
    switchTo(SCREEN_MODULE_DETAIL);
    return;
  }

  // Slot toggles (7 chips)
  for (int s = 0; s < NUM_TIME_SLOTS; s++) {
    int col = s % 4;
    int row = s / 4;
    int px = 22 + col * 112;
    int py = 204 + row * 30;
    if (x >= px && x <= px + 104 && y >= py && y <= py + 26) {
      moduleToggleSlot(editModIdx, s);
      switchTo(SCREEN_MODULE_DETAIL);
      return;
    }
  }

  // Save: 140-340, 272-314
  if (x >= 140 && x <= 340 && y >= 272 && y <= 314) {
    schedulerSave();
    switchTo(SCREEN_MODULES);
    return;
  }
}

// ============================================================
// MANUAL DISPENSE SCREEN — Grid of 6 modules
// ============================================================
static void drawManualDispense() {
  auto &lcd = canvas;

  // Header
  lcd.fillRect(0, 0, 480, 40, COL_DANGER);
  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_TEXT_INV, COL_DANGER);
  lcd.drawString("Manual Dispense", 240, 20);
  btn(5, 5, 60, 30, "Back", COL_CARD, COL_TEXT);

  // Draw 6 modules as buttons
  int cw = 140, ch = 100;
  for (int i = 0; i < NUM_MODULES; i++) {
    int col = i % 3;
    int row = i / 3;
    int cx = 20 + col * 150;
    int cy = 60 + row * 115;

    MedModule &mod = moduleGet(i);
    bool active = servoIsManualActive(i);

    // Card bg
    lcd.fillRoundRect(cx, cy, cw, ch, 8, active ? COL_WARN : COL_CARD);

    // Module name
    lcd.setFont(&fonts::FreeSans9pt7b);
    lcd.setTextDatum(middle_center);
    lcd.setTextColor(active ? COL_BG : COL_PRIMARY,
                     active ? COL_WARN : COL_CARD);
    lcd.drawString(mod.name, cx + cw / 2, cy + 30);

    // Action button inside card
    int bx = cx + 20;
    int by = cy + 60;
    btn(bx, by, 100, 30, active ? "STOP" : "DISPENSE",
        active ? COL_DANGER : COL_PRIMARY, active ? COL_TEXT : COL_BG);
  }
}

// ============================================================
// TOUCH: MANUAL DISPENSE
// ============================================================
static void touchManualDispense(int x, int y) {
  // Back: 5-65, 5-35
  if (x <= 65 && y <= 40) {
    switchTo(SCREEN_HOME);
    return;
  }

  int cw = 140, ch = 100;
  for (int i = 0; i < NUM_MODULES; i++) {
    int col = i % 3;
    int row = i / 3;
    int cx = 20 + col * 150;
    int cy = 60 + row * 115;

    // Action button inside card
    int bx = cx + 20;
    int by = cy + 60;

    if (x >= bx && x <= bx + 100 && y >= by && y <= by + 30) {
      if (manualCb) {
        manualCb(i);
        switchTo(SCREEN_MANUAL_DISPENSE); // Redraw
      }
      return;
    }
  }
}

// ============================================================
// CONFIRM DISPENSE SCREEN
// ============================================================
static void drawConfirmDispense() {
  auto &lcd = canvas;

  // Header
  lcd.fillRect(0, 0, 480, 44, COL_WARN);
  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_BG, COL_WARN);
  lcd.drawString("Medicine Time!", 240, 22);

  // Time Slot Info
  lcd.setFont(&fonts::FreeSans12pt7b);
  lcd.setTextColor(COL_PRIMARY, COL_BG);
  char buf[64];
  if (confirmSlotIdx >= 0) {
    TimeSlot &ts = timeSlotGet(confirmSlotIdx);
    sprintf(buf, "%s (%02d:%02d)", periodName[confirmSlotIdx / 2], ts.hour,
            ts.minute);
    lcd.drawString(buf, 240, 75);
  }

  // Countdown timer
  long elapsed = millis() - confirmStartMs;
  long remaining = (10 * 60 * 1000UL) - elapsed;
  if (remaining < 0)
    remaining = 0;
  int rm = (remaining / 1000) / 60;
  int rs = (remaining / 1000) % 60;

  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextColor(COL_DANGER, COL_BG);
  sprintf(buf, "Auto-cancel in %02d:%02d", rm, rs);
  lcd.drawString(buf, 240, 110);

  // Confirm Button (Big)
  lcd.fillRoundRect(60, 140, 360, 90, 8, COL_SUCCESS);
  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextColor(COL_TEXT_INV, COL_SUCCESS);
  lcd.drawString("PRESS TO DISPENSE", 240, 185);

  // Cancel Button
  btn(190, 250, 100, 40, "Cancel", COL_CARD, COL_TEXT_DIM);
}

// ============================================================
// TOUCH: CONFIRM DISPENSE
// ============================================================
static void touchConfirmDispense(int x, int y) {
  // Confirm Button (60, 140, 360, 90)
  if (x > 60 && x < 420 && y > 140 && y < 230) {
    if (confirmCb) {
      confirmCb(confirmSlotIdx);
    } else {
      switchTo(SCREEN_HOME);
    }
    return;
  }

  // Cancel Button (190, 250, 100, 40)
  if (x > 190 && x < 290 && y > 250 && y < 290) {
    switchTo(SCREEN_HOME);
  }
}

// ============================================================
// WIFI MENU
// ============================================================
static void drawWifiMenu() {
  auto &lcd = canvas;
  lcd.fillScreen(COL_BG);
  lcd.fillRect(0, 0, 480, 40, COL_PRIMARY);
  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_TEXT_INV, COL_PRIMARY);
  lcd.drawString("WiFi Setup", 240, 20);
  btn(5, 5, 60, 30, "Back", COL_CARD, COL_TEXT);

  btn(40, 80, 400, 60, "1. Quick Connect (Mobile Captive Portal)", COL_PRIMARY,
      COL_BG);
  btn(40, 160, 400, 60, "2. Manual Connect (On-Screen Keyboard)", COL_CARD,
      COL_TEXT);
  btn(140, 250, 200, 40, "Forget WiFi Network", COL_DANGER, COL_TEXT_INV);
}

static void touchWifiMenu(int x, int y) {
  if (x <= 65 && y <= 40) {
    switchTo(SCREEN_HOME);
    return;
  }
  if (x >= 40 && x <= 440 && y >= 80 && y <= 140) {
    switchTo(SCREEN_WIFI_PORTAL);
    return;
  }
  if (x >= 40 && x <= 440 && y >= 160 && y <= 220) {
    wifiNeedsScan = true;
    switchTo(SCREEN_WIFI_SCAN);
    return;
  }
  if (x >= 140 && x <= 340 && y >= 250 && y <= 290) {
    wifiForget();
    switchTo(SCREEN_HOME);
    return;
  }
}

// ============================================================
// WIFI PORTAL (Mobile connect)
// ============================================================
static void drawWifiPortal() {
  auto &lcd = canvas;
  lcd.fillRect(0, 0, 480, 40, COL_PRIMARY);
  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_TEXT_INV, COL_PRIMARY);
  lcd.drawString("Connect via Phone", 240, 20);

  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextColor(COL_TEXT, COL_BG);
  lcd.drawString("1. Connect your phone to WiFi: Med-Dispenser", 240, 100);
  lcd.drawString("2. A webpage will open automatically.", 240, 140);
  lcd.drawString("3. Select your home network and enter the password.", 240,
                 180);

  lcd.setTextColor(COL_DANGER, COL_BG);
  lcd.drawString("Look at your phone now! (Timeout in 3 mins)", 240, 250);
}

static void touchWifiPortal(int x, int y) {
  // Ignored while portal is running.
}

// ============================================================
// WIFI SCAN
// ============================================================
static void drawWifiScan() {
  auto &lcd = canvas;
  lcd.fillScreen(COL_BG);
  lcd.fillRect(0, 0, 480, 40, COL_PRIMARY);
  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_TEXT_INV, COL_PRIMARY);
  lcd.drawString("Select Network", 240, 20);
  btn(5, 5, 60, 30, "Back", COL_CARD, COL_TEXT);

  if (wifiScanning) {
    lcd.setTextColor(COL_TEXT);
    lcd.drawString("Scanning...", 240, 150);
    return;
  }
  if (wifiScanCount == 0) {
    lcd.setTextColor(COL_TEXT_DIM);
    lcd.drawString("No networks found", 240, 150);
    btn(190, 200, 100, 40, "Rescan", COL_PRIMARY, COL_TEXT_INV);
    return;
  }

  int startIdx = wifiScanPage * 4;
  for (int i = 0; i < 4; i++) {
    int idx = startIdx + i;
    if (idx >= wifiScanCount)
      break;
    int y = 55 + i * 50;
    btn(40, y, 400, 40, wifiNetworks[idx].c_str(), COL_CARD, COL_TEXT);
  }

  // Nav
  if (wifiScanPage > 0)
    btn(10, 270, 80, 40, "< Prev", COL_BTN, COL_TEXT);
  if (startIdx + 4 < wifiScanCount)
    btn(390, 270, 80, 40, "Next >", COL_BTN, COL_TEXT);
  btn(200, 270, 80, 40, "Rescan", COL_PRIMARY, COL_TEXT_INV);
}

static void touchWifiScan(int x, int y) {
  if (wifiScanning)
    return;
  if (x <= 65 && y <= 40) {
    switchTo(SCREEN_WIFI_MENU);
    return;
  }

  // Rescan empty
  if (wifiScanCount == 0 && x >= 190 && x <= 290 && y >= 200 && y <= 240) {
    wifiNeedsScan = true;
    switchTo(SCREEN_WIFI_SCAN);
    return;
  }

  // Nav
  int startIdx = wifiScanPage * 4;
  if (wifiScanPage > 0 && x >= 10 && x <= 90 && y >= 270 && y <= 310) {
    wifiScanPage--;
    switchTo(SCREEN_WIFI_SCAN);
    return;
  }
  if (startIdx + 4 < wifiScanCount && x >= 390 && x <= 470 && y >= 270 &&
      y <= 310) {
    wifiScanPage++;
    switchTo(SCREEN_WIFI_SCAN);
    return;
  }
  if (wifiScanCount > 0 && x >= 200 && x <= 280 && y >= 270 && y <= 310) {
    wifiNeedsScan = true;
    switchTo(SCREEN_WIFI_SCAN);
    return;
  }

  // Networks
  for (int i = 0; i < 4; i++) {
    int idx = startIdx + i;
    if (idx >= wifiScanCount)
      break;
    int by = 55 + i * 50;
    if (x >= 40 && x <= 440 && y >= by && y <= by + 40) {
      selectedSSID = wifiNetworks[idx];
      inputPassword = "";
      oskShift = false;
      switchTo(SCREEN_WIFI_OSK);
      return;
    }
  }
}

// ============================================================
// WIFI OSK
// ============================================================
static const char *keys[4][10] = {
    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
    {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"},
    {"a", "s", "d", "f", "g", "h", "j", "k", "l", ""},
    {"^", "z", "x", "c", "v", "b", "n", "m", "DEL", ""}};
static const char *keysShift[4][10] = {
    {"!", "@", "#", "$", "%", "^", "&", "*", "(", ")"},
    {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
    {"A", "S", "D", "F", "G", "H", "J", "K", "L", ""},
    {"^", "Z", "X", "C", "V", "B", "N", "M", "DEL", ""}};

static void drawWifiOSK() {
  auto &lcd = canvas;
  lcd.fillScreen(COL_BG);
  lcd.fillRect(0, 0, 480, 40, COL_PRIMARY);
  lcd.setFont(&fonts::FreeSansBold12pt7b);
  lcd.setTextDatum(middle_center);
  lcd.setTextColor(COL_TEXT_INV, COL_PRIMARY);
  String title = "SSID: " + selectedSSID;
  lcd.drawString(title.substring(0, 25).c_str(), 240, 20);
  btn(5, 5, 80, 30, "Cancel", COL_CARD, COL_TEXT);
  btn(390, 5, 85, 30, "Connect", COL_SUCCESS, COL_TEXT_INV);

  // Input Box
  lcd.fillRoundRect(20, 50, 440, 40, 5, COL_CARD);
  lcd.setFont(&fonts::FreeSans12pt7b);
  lcd.setTextDatum(middle_left);
  lcd.setTextColor(COL_TEXT, COL_CARD);
  String disp = inputPassword + "_";
  lcd.drawString(disp.c_str(), 30, 70);

  // Keyboard
  int startX = 10;
  int startY = 100;
  int keyW = 42;
  int keyH = 45;
  int gap = 4;
  lcd.setFont(&fonts::FreeSans9pt7b);
  lcd.setTextDatum(middle_center);

  for (int r = 0; r < 4; r++) {
    int rowOffset = (r == 2) ? 20 : 0;
    for (int c = 0; c < 10; c++) {
      const char *k = oskShift ? keysShift[r][c] : keys[r][c];
      if (k[0] == '\0')
        continue; // Skip empty
      int x = startX + rowOffset + c * (keyW + gap);
      int y = startY + r * (keyH + gap);

      int w = keyW;
      if (r == 3 && c == 8)
        w = keyW * 2 + gap; // DEL

      if (r == 3 && c == 0) { // Shift
        btn(x, y, w, keyH, k, oskShift ? COL_PRIMARY : COL_BTN, COL_TEXT);
      } else if (r == 3 && c == 8) {
        btn(x, y, w, keyH, k, COL_DANGER, COL_TEXT);
      } else {
        btn(x, y, w, keyH, k, COL_BTN, COL_TEXT);
      }
    }
  }
}

static void touchWifiOSK(int x, int y) {
  // Navigation
  if (y <= 40) {
    if (x <= 90) { // Cancel
      switchTo(SCREEN_WIFI_SCAN);
      return;
    }
    if (x >= 390) { // Connect
      wifiConnectManual(selectedSSID.c_str(), inputPassword.c_str());
      switchTo(SCREEN_HOME);
      return;
    }
  }

  // Keyboard Touch
  int startX = 10;
  int startY = 100;
  int keyW = 42;
  int keyH = 45;
  int gap = 4;

  for (int r = 0; r < 4; r++) {
    int rowOffset = (r == 2) ? 20 : 0;
    for (int c = 0; c < 10; c++) {
      int px = startX + rowOffset + c * (keyW + gap);
      int py = startY + r * (keyH + gap);
      int w = keyW;
      if (r == 3 && c == 8)
        w = keyW * 2 + gap; // DEL

      const char *k = oskShift ? keysShift[r][c] : keys[r][c];
      if (k[0] == '\0')
        continue;

      if (x >= px && x <= px + w && y >= py && y <= py + keyH) {
        if (r == 3 && c == 0) {
          oskShift = !oskShift;
        } else if (r == 3 && c == 8) {
          if (inputPassword.length() > 0) {
            inputPassword.remove(inputPassword.length() - 1);
          }
        } else {
          if (inputPassword.length() < 30) {
            inputPassword += k;
          }
        }
        switchTo(SCREEN_WIFI_OSK); // Redraw
        return;
      }
    }
  }
}
