#include "display_module.h"
#include <Wire.h>

// ============================================================
// LovyanGFX Configuration
// ============================================================
LGFX::LGFX(void) {
  // --- SPI Bus ---
  {
    auto cfg = _bus_instance.config();
    cfg.spi_host = SPI2_HOST;
    cfg.spi_mode = 0;
    cfg.freq_write = 16000000;
    cfg.freq_read = 8000000;
    cfg.spi_3wire = false;
    cfg.use_lock = true;
    cfg.dma_channel = SPI_DMA_CH_AUTO;
    cfg.pin_sclk = LCD_SCK_PIN;
    cfg.pin_mosi = LCD_MOSI_PIN;
    cfg.pin_miso = LCD_MISO_PIN;
    cfg.pin_dc = LCD_DC_PIN;
    _bus_instance.config(cfg);
    _panel_instance.setBus(&_bus_instance);
  }

  // --- LCD Panel (ST7796S) ---
  {
    auto cfg = _panel_instance.config();
    cfg.pin_cs = LCD_CS_PIN;
    cfg.pin_rst = LCD_RST_PIN;
    cfg.pin_busy = -1;
    cfg.panel_width = 320;
    cfg.panel_height = 480;
    cfg.offset_x = 0;
    cfg.offset_y = 0;
    cfg.offset_rotation = 0;
    cfg.dummy_read_pixel = 8;
    cfg.dummy_read_bits = 1;
    cfg.readable = false;
    cfg.invert = false;
    cfg.rgb_order = false;
    cfg.dlen_16bit = false;
    cfg.bus_shared = false;
    _panel_instance.config(cfg);
  }

  // --- Touch Panel (FT6236 via I2C) ---
  {
    auto cfg = _touch_instance.config();
    cfg.x_min = 0;
    cfg.x_max = 319;
    cfg.y_min = 0;
    cfg.y_max = 479;
    cfg.pin_int = CTP_INT_PIN;
    cfg.pin_rst = -1;
    cfg.bus_shared = true; // MUST be true so Wire.begin() works
    cfg.offset_rotation = 0;
    cfg.i2c_port = 0;
    cfg.i2c_addr = FT6236_ADDR;
    cfg.pin_sda = I2C_SDA_PIN;
    cfg.pin_scl = I2C_SCL_PIN;
    cfg.freq = 400000;
    _touch_instance.config(cfg);
    _panel_instance.setTouch(&_touch_instance);
  }

  setPanel(&_panel_instance);
}

// ============================================================
// Global display instance
// ============================================================
static LGFX display;

// ============================================================
LGFX &getDisplay(void) { return display; }

// ============================================================
// displaySetup
// ============================================================
void displaySetup(void) {
  pinMode(LCD_RST_PIN, OUTPUT);
  digitalWrite(LCD_RST_PIN, HIGH);
  delay(10);
  digitalWrite(LCD_RST_PIN, LOW);
  delay(50);
  digitalWrite(LCD_RST_PIN, HIGH);
  delay(200);

  display.init();
  delay(100);
  // Landscape mode: 480 x 320 (flipped)
  display.setRotation(3);
  display.setBrightness(255);
  display.setColorDepth(16);
  display.fillScreen(TFT_BLACK);

  Serial.printf("[Display] Init OK — %dx%d\n", display.width(),
                display.height());
}

// displayLoop — no longer used (ui_manager handles everything)
void displayLoop(void) {}
