#pragma once

#include "config.h"
#include <LovyanGFX.hpp>

// ============================================================
// LovyanGFX Display Class for ST7796S SPI + FT6236 I2C Touch
// ============================================================
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Touch_FT5x06 _touch_instance;

public:
  LGFX(void);
};

// --- Public API ---
void displaySetup(void);
void displayLoop(void);
LGFX &getDisplay(void);
