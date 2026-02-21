#include <Arduino.h>
#include <Wire.h>

#define I2C_SDA 8
#define I2C_SCL 9

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n=== I2C Scanner ===");
  Serial.println("Scanning I2C bus...\n");
  
  Wire.begin(I2C_SDA, I2C_SCL);
  
  byte count = 0;
  
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      
      // แสดงชื่ออุปกรณ์ที่รู้จัก
      Serial.print(" (");
      if (address == 0x20 || address == 0x27) Serial.print("PCF8574");
      else if (address == 0x40) Serial.print("PCA9685");
      else if (address == 0x68) Serial.print("DS3231 RTC");
      else Serial.print("Unknown");
      Serial.println(")");
      
      count++;
    }
  }
  
  Serial.println("------------------");
  if (count == 0) {
    Serial.println("No I2C devices found!");
  } else {
    Serial.print("Found ");
    Serial.print(count);
    Serial.println(" device(s)");
  }
  
  Serial.println("\n=== Scan Complete ===");
  Serial.println("Upload your main code now.");
}

void loop() {
  // ไม่ต้องทำอะไร
}
