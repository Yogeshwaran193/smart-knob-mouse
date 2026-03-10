/*
 * Phase 1 — IMU Cursor (ESP32 Side) — BMI160
 * 
 * Reads BMI160 and sends CSV over Serial for Python cursor control.
 * Format: IMU,accX,accY,accZ,gyroX,gyroY,gyroZ\n
 * 
 * Wiring:
 *   SDA → GPIO 37, SCL → GPIO 38
 *   3V3 → 3.3V, GND → GND
 * 
 * Author: Yogeshwaran
 */

#include <Wire.h>

#define IMU_SDA     37
#define IMU_SCL     38
#define BMI160_ADDR 0x69

int16_t accX, accY, accZ;
int16_t gyroX, gyroY, gyroZ;

uint8_t readReg(uint8_t reg) {
  Wire1.beginTransmission(BMI160_ADDR);
  Wire1.write(reg);
  Wire1.endTransmission(false);
  Wire1.requestFrom(BMI160_ADDR, (uint8_t)1);
  if (Wire1.available()) return Wire1.read();
  return 0xFF;
}

void writeReg(uint8_t reg, uint8_t val) {
  Wire1.beginTransmission(BMI160_ADDR);
  Wire1.write(reg);
  Wire1.write(val);
  Wire1.endTransmission();
}

void readIMU() {
  Wire1.beginTransmission(BMI160_ADDR);
  Wire1.write(0x0C);  // Start from gyro X LSB
  Wire1.endTransmission(false);
  Wire1.requestFrom(BMI160_ADDR, (uint8_t)12);

  // BMI160: LSB first
  gyroX = Wire1.read() | (Wire1.read() << 8);
  gyroY = Wire1.read() | (Wire1.read() << 8);
  gyroZ = Wire1.read() | (Wire1.read() << 8);
  accX  = Wire1.read() | (Wire1.read() << 8);
  accY  = Wire1.read() | (Wire1.read() << 8);
  accZ  = Wire1.read() | (Wire1.read() << 8);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire1.begin(IMU_SDA, IMU_SCL);
  Wire1.setClock(400000);

  // Verify chip
  if (readReg(0x00) != 0xD1) {
    Serial.println("ERROR: BMI160 not found!");
    while (1) delay(1000);
  }

  // Soft reset
  writeReg(0x7E, 0xB6);
  delay(200);

  // Accel normal mode
  writeReg(0x7E, 0x11);
  delay(50);

  // Gyro normal mode
  writeReg(0x7E, 0x15);
  delay(100);

  // Accel: 100Hz, +/-2g
  writeReg(0x40, 0x28);
  writeReg(0x41, 0x03);

  // Gyro: 100Hz, +/-250 deg/s
  writeReg(0x42, 0x28);
  writeReg(0x43, 0x03);

  Serial.println("READY");
}

void loop() {
  readIMU();

  Serial.printf("IMU,%d,%d,%d,%d,%d,%d\n",
    accX, accY, accZ, gyroX, gyroY, gyroZ);

  delay(10);  // 100Hz
}
