// ===== MINIMAL WIFI TEST FOR ESP32-P4-NANO =====
// ไม่มี I2C, ไม่มี Display, ไม่มี Servo — แค่ WiFi อย่างเดียว
// ถ้ายังเชื่อมไม่ได้ = ปัญหาที่ board/platform

#include <Arduino.h>
#include <WiFi.h>

#define WIFI_SSID "iPhone 17 air"
#define WIFI_PASS "peek2003"

void setup() {
  Serial.begin(115200);
  unsigned long s = millis();
  while (!Serial && millis() - s < 2000) {
  }

  Serial.println("\n\n===== ESP32-P4-NANO WiFi Test =====");
  Serial.printf("Chip: %s\n", ESP.getChipModel());
  Serial.printf("SDK: %s\n", ESP.getSdkVersion());
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());

  // Step 1: Set mode
  Serial.println("\n[1] WiFi.mode(WIFI_STA)...");
  WiFi.mode(WIFI_STA);
  Serial.println("    Done!");

  // Step 2: Disconnect (clean state)
  Serial.println("[2] WiFi.disconnect(true)...");
  WiFi.disconnect(true);
  Serial.println("    Done!");

  // Step 3: Scan
  Serial.println("[3] Scanning networks...");
  int n = WiFi.scanNetworks();
  Serial.printf("    Found %d networks:\n", n);
  for (int i = 0; i < n; i++) {
    Serial.printf("    %2d: %-32s %4ddBm CH%d %s\n", i + 1,
                  WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                  WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN"
                                                           : "SECURED");
  }
  WiFi.scanDelete();

  if (n <= 0) {
    Serial.println("\n*** NO NETWORKS FOUND - C6 RADIO NOT WORKING! ***");
    Serial.println("*** Check esp_hosted driver / platform version ***");
    return;
  }

  // Step 4: Connect
  Serial.printf("\n[4] WiFi.begin('%s', '%s')...\n", WIFI_SSID, WIFI_PASS);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  // Step 5: Wait 60 seconds
  Serial.println("[5] Waiting for connection (60s)...");
  unsigned long start = millis();
  int lastStatus = -1;
  while (WiFi.status() != WL_CONNECTED && millis() - start < 60000) {
    int st = WiFi.status();
    if (st != lastStatus) {
      const char *names[] = {"IDLE",        "NO_SSID",      "SCAN_DONE",
                             "CONNECTED",   "CONNECT_FAIL", "CONN_LOST",
                             "DISCONNECTED"};
      const char *nm = (st >= 0 && st <= 6) ? names[st] : "UNKNOWN";
      Serial.printf("    [%3lus] Status=%d (%s)\n", (millis() - start) / 1000,
                    st, nm);
      lastStatus = st;
    }
    Serial.print(".");
    unsigned long w = millis();
    while (millis() - w < 500) {
    } // non-blocking wait
  }
  Serial.println();

  // Step 6: Result
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n===== SUCCESS! WiFi CONNECTED! =====");
    Serial.printf("IP:   %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
    Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
    Serial.printf("GW:   %s\n", WiFi.gatewayIP().toString().c_str());
  } else {
    Serial.println("\n===== FAILED! WiFi NOT CONNECTED =====");
    Serial.printf("Final status: %d\n", WiFi.status());
    Serial.println("Possible causes:");
    Serial.println("  1. Wrong SSID/password");
    Serial.println("  2. Hotspot not broadcasting on 2.4GHz");
    Serial.println("  3. esp_hosted driver issue (platform version)");
    Serial.println("  4. C6 firmware needs update");
  }
}

void loop() {
  static unsigned long last = 0;
  if (millis() - last > 5000) {
    Serial.printf("[Loop] WiFi=%d | Heap=%d\n", WiFi.status(),
                  ESP.getFreeHeap());
    last = millis();
  }
}
