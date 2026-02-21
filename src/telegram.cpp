#include "telegram.h"
#include "config.h"
#include "logging.h"
#include "offline_module.h" // Added for getOfflineLogs/clearOfflineLogs
#include "secrets.h"
#include <UniversalTelegramBot.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// ============================================
// Telegram Bot Implementation
// ============================================

WiFiClientSecure securedClient;
UniversalTelegramBot bot(BOT_TOKEN, securedClient);

static TelegramCommandCallback onTelegramCommand = nullptr;
static unsigned long lastBotCheck = 0;

void setTelegramCommandCallback(TelegramCommandCallback callback) {
  onTelegramCommand = callback;
}

// --- Setup ---
void telegramSetup() {
  securedClient.setInsecure();
  LOG_INFO("[Telegram] Bot initialized");
}

// --- Send Notification ---
// --- Send Notification ---
void sendTelegramNotification(String message) {
  if (WiFi.status() != WL_CONNECTED) {
    LOG_WARN("[Telegram] No WiFi, message not sent");
    return;
  }

  // Ensure client is insecure (skip certificate check)
  securedClient.setInsecure();

  LOG_DEBUG("[Telegram] Sending: %s", message.c_str());

  if (bot.sendMessage(OWNER_CHAT_ID, message, "")) {
    LOG_INFO("[Telegram] Message sent OK");
  } else {
    LOG_ERROR("[Telegram] Failed to send message");
  }
}

// --- Handle Incoming Messages ---
void handleTelegramMessages() {
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  int batchCount = 0; // Bug8 fix: จำกัดจำนวนรอบป้องกัน infinite loop

  while (numNewMessages && batchCount < 5) {
    for (int i = 0; i < numNewMessages; i++) {
      String chatId = String(bot.messages[i].chat_id);
      String text = bot.messages[i].text;

      LOG_INFO("[Telegram] From %s: %s", chatId.c_str(), text.c_str());

      // เฉพาะ owner เท่านั้น
      if (chatId == String(OWNER_CHAT_ID)) {
        if (text == "/logs") {
          sendTelegramNotification(getOfflineLogs());
        } else if (text == "/clear_logs") {
          clearOfflineLogs();
          sendTelegramNotification("Offline logs cleared.");
        } else if (onTelegramCommand) {
          onTelegramCommand(text);
        }
      }
    }
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    batchCount++;
  }
}

// --- Loop (non-blocking) ---
void telegramLoop() {
  if (WiFi.status() != WL_CONNECTED)
    return;

  if (millis() - lastBotCheck > BOT_CHECK_INTERVAL) {
    handleTelegramMessages();
    lastBotCheck = millis();
  }
}
