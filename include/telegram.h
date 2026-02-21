#ifndef TELEGRAM_H
#define TELEGRAM_H

#include <Arduino.h>

// ============================================
// Telegram Bot Module
// ============================================

void telegramSetup();
void telegramLoop();
void sendTelegramNotification(String message);

// Callback สำหรับคำสั่งจาก Telegram
typedef void (*TelegramCommandCallback)(String command);
void setTelegramCommandCallback(TelegramCommandCallback callback);

#endif // TELEGRAM_H
