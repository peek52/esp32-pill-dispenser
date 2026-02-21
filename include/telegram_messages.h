#ifndef TELEGRAM_MESSAGES_H
#define TELEGRAM_MESSAGES_H

// ============================================
// Telegram Notification Messages
// สามารถแก้ไขข้อความที่จะส่งเข้า Telegram ได้ที่นี่
// ============================================

// --- ข้อความแจ้งเตือนต่างๆ ---
#define MSG_TIME_TO_TAKE_PILL "💊 ถึงเวลาทานยา!" // เมื่อถึงเวลาทานยา
#define MSG_DISPENSE_SUCCESS "✅ จ่ายยาสำเร็จ"    // เมื่อจ่ายยาสำเร็จ
#define MSG_DISPENSE_FAIL "⚠️ ไม่พบยา"     // เมื่อจ่ายยาไม่สำเร็จ (ไม่มียาตกผ่าน sensor)
#define MSG_DISPENSING "💊 กำลังจ่ายยา..." // เมื่อกำลังเริ่มจ่ายยา (Manual/Command)
#define MSG_SYSTEM_ONLINE                                                      \
  "🟢 เครื่องจ่ายยาพร้อมแล้ว" // เมื่อระบบเริ่มทำงานและต่อเน็ตได้ (ตามด้วยชื่อยา)
#define MSG_SYSTEM_STATUS "📊 สถานะระบบ" // หัวข้อสถานะระบบ
#define MSG_COMMAND_LIST "📋 คำสั่ง:"      // หัวข้อรายการคำสั่ง help

// --- ข้อความในคำสั่ง /status ---
#define MSG_LABEL_TIME "เวลา: "
#define MSG_LABEL_MEDICINE "ยา: "
#define MSG_LABEL_SCHEDULE "ตารางเวลา: "
#define MSG_LABEL_WIFI "WiFi: "
#define MSG_LABEL_MODE "Mode: "
#define MSG_STATUS_ON "เปิด"
#define MSG_STATUS_OFF "ปิด"

#endif // TELEGRAM_MESSAGES_H
