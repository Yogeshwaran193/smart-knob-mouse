/*
 * I2C Address Scanner — ESP32-S3
 * 
 * Scans BOTH I2C buses for all connected devices.
 * 
 * Bus 1 (Wire1): IMU bus     → SDA=37, SCL=38  (expect 0x68 = MPU6050)
 * Bus 0 (Wire):  AS5600 bus  → SDA=8,  SCL=9   (expect 0x36 = AS5600)
 * 
 * Author: Yogeshwaran
 */

#include <Wire.h>

// Pin config from your documentation
#define IMU_SDA    37
#define IMU_SCL    38
#define AS5600_SDA  8
#define AS5600_SCL  9

void scanBus(TwoWire &bus, const char* name) {
  int found = 0;

  Serial.printf("\n--- Scanning %s ---\n", name);

  for (uint8_t addr = 1; addr < 127; addr++) {
    bus.beginTransmission(addr);
    uint8_t error = bus.endTransmission();

    if (error == 0) {
      Serial.printf("  Found device at 0x%02X", addr);

      // Label known addresses
      if (addr == 0x68) Serial.print("  ← MPU6050 (IMU)");
      if (addr == 0x69) Serial.print("  ← MPU6050 (AD0=HIGH)");
      if (addr == 0x36) Serial.print("  ← AS5600 (encoder)");
      if (addr == 0x3C) Serial.print("  ← OLED display");
      if (addr == 0x57) Serial.print("  ← EEPROM");
      if (addr == 0x50) Serial.print("  ← EEPROM");

      Serial.println();
      found++;
    }
  }

  if (found == 0) {
    Serial.println("  No devices found! Check wiring.");
  } else {
    Serial.printf("  %d device(s) found.\n", found);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== I2C Address Scanner ===");

  // Start both buses
  Wire.begin(AS5600_SDA, AS5600_SCL);
  Wire1.begin(IMU_SDA, IMU_SCL);

  // Scan both
  scanBus(Wire1, "Bus 1: IMU (SDA=37, SCL=38)");
  scanBus(Wire,  "Bus 0: AS5600 (SDA=8, SCL=9)");

  Serial.println("\nDone. Press RST to scan again.");
}

void loop() {
  delay(1000);
}
