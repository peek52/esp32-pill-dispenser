#include "audio_manager.h"
#include "config.h"
#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>
#include <HardwareSerial.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

static DFRobotDFPlayerMini dfPlayer;
static bool playerReady = false;
static QueueHandle_t audioQueue = NULL;

// --- Persistence for Volume ---
#include <Preferences.h>
static Preferences audioPrefs;
static int currentVolume = 10;

static void audioTask(void *pvParameters) {
  int trackToPlay;
  for (;;) {
    if (xQueueReceive(audioQueue, &trackToPlay, pdMS_TO_TICKS(50)) == pdPASS) {
      if (playerReady) {
        Serial.printf("[AUDIO] Task playing track %04d.mp3 (Vol: %d)\n",
                      trackToPlay, currentVolume);

        // Brownout protection: Give the power supply a tiny moment to stabilize
        // before the 3W amplifier spikes the current
        vTaskDelay(pdMS_TO_TICKS(100));

        dfPlayer.volume(currentVolume);

        // Another tiny delay before play command
        vTaskDelay(pdMS_TO_TICKS(50));

        // Prevent watchdog trigger if dfPlayer.play() is blocking internally
        esp_task_wdt_reset();
        dfPlayer.play(trackToPlay);
      }
    }

    // Crucial: Yield CPU time to the idle task to prevent watchdog reset
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void audioSetup() {
  Serial.println("[AUDIO] Initializing DFPlayer Mini...");

  // Load Volume
  audioPrefs.begin("audio", false);
  currentVolume = audioPrefs.getInt("vol", 10);
  audioPrefs.end();

  // ESP32-P4 UART1
  Serial1.begin(9600, SERIAL_8N1, DFPLAYER_ESP_RX_PIN, DFPLAYER_ESP_TX_PIN);

  // Give the module time to boot up
  delay(100);

  if (!dfPlayer.begin(Serial1, /*isACK = */ false, /*doReset = */ false)) {
    Serial.println(
        "[AUDIO] Error: DFPlayer Mini not found or SD card missing!");
    playerReady = false;
  } else {
    Serial.println("[AUDIO] DFPlayer Mini Online.");
    playerReady = true;
    dfPlayer.volume(currentVolume);
  }

  // Create queue and task regardless of ready state, so we don't crash if
  // called later
  audioQueue = xQueueCreate(5, sizeof(int));
  if (audioQueue != NULL) {
    xTaskCreatePinnedToCore(audioTask, "AudioTask", 4096, NULL, 1, NULL, 0);
  }
}

void audioPlay(int track) {
  if (audioQueue != NULL) {
    // Non-blocking send
    if (xQueueSend(audioQueue, &track, (TickType_t)10) != pdPASS) {
      Serial.println("[AUDIO] Failed to send to queue (Queue Full)");
    }
  } else {
    Serial.println("[AUDIO] Queue not initialized");
  }
}

void audioSetVolume(int volume) {
  if (volume < 0)
    volume = 0;
  if (volume > 30)
    volume = 30; // DFPlayer max is 30
  currentVolume = volume;

  audioPrefs.begin("audio", false);
  audioPrefs.putInt("vol", currentVolume);
  audioPrefs.end();

  if (playerReady) {
    dfPlayer.volume(currentVolume);
    Serial.printf("[AUDIO] Volume dynamically set to %d\n", currentVolume);
  }
}

int audioGetVolume() { return currentVolume; }
