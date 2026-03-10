/*
 * Phase 1 — IMU Test for BMI160 (Bosch)
 * 
 * Your board has a BMI160, NOT an MPU6050.
 * Chip ID register 0x00 = 0xD1 confirms this.
 * 
 * BMI160 has a completely different register map:
 *   - Chip ID at 0x00 (not 0x75)
 *   - Data is LSB first (not MSB first)
 *   - Uses CMD register (0x7E) to change power modes
 *   - Accel data at 0x12-0x17
 *   - Gyro data at 0x0C-0x11
 * 
 * Wiring:
 *   SDA → GPIO 37, SCL → GPIO 38
 *   3V3 → 3.3V, GND → GND
 * 
 * Author: Yogeshwaran
 */

#include <Wire.h>

#define IMU_SDA  37
#define IMU_SCL  38
#define BMI160_ADDR  0x69  // Your board has SDO=HIGH

// ── BMI160 Register Addresses ──
#define REG_CHIP_ID    0x00  // Should read 0xD1
#define REG_ERR_REG    0x02
#define REG_PMU_STATUS 0x03
#define REG_DATA_GYR_X 0x0C  // Gyro X LSB (12 bytes: gx,gy,gz,ax,ay,az)
#define REG_DATA_ACC_X 0x12  // Accel X LSB
#define REG_ACC_CONF   0x40
#define REG_ACC_RANGE  0x41
#define REG_GYR_CONF   0x42
#define REG_GYR_RANGE  0x43
#define REG_CMD        0x7E

// Raw data
int16_t accX, accY, accZ;
int16_t gyroX, gyroY, gyroZ;

// ── I2C helpers ──
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

// ── Read 12 bytes of sensor data (gyro + accel) ──
void readSensors() {
  Wire1.beginTransmission(BMI160_ADDR);
  Wire1.write(REG_DATA_GYR_X);  // Start from gyro X LSB
  Wire1.endTransmission(false);
  Wire1.requestFrom(BMI160_ADDR, (uint8_t)12);

  // BMI160 is LSB first (opposite of MPU6050!)
  gyroX = Wire1.read() | (Wire1.read() << 8);
  gyroY = Wire1.read() | (Wire1.read() << 8);
  gyroZ = Wire1.read() | (Wire1.read() << 8);
  accX  = Wire1.read() | (Wire1.read() << 8);
  accY  = Wire1.read() | (Wire1.read() << 8);
  accZ  = Wire1.read() | (Wire1.read() << 8);
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("=== Phase 1: BMI160 IMU Test ===\n");

  Wire1.begin(IMU_SDA, IMU_SCL);
  Wire1.setClock(400000);

  // ── Verify chip ID ──
  uint8_t chipId = readReg(REG_CHIP_ID);
  Serial.printf("Chip ID: 0x%02X", chipId);
  if (chipId == 0xD1) {
    Serial.println(" ← BMI160 confirmed!");
  } else {
    Serial.printf(" ← expected 0xD1, got 0x%02X\n", chipId);
    Serial.println("Continuing anyway...");
  }

  // ── Soft reset ──
  Serial.print("Resetting...");
  writeReg(REG_CMD, 0xB6);  // Soft reset command
  delay(200);  // BMI160 needs time after reset
  Serial.println(" done.");

  // ── Set accelerometer to normal mode ──
  writeReg(REG_CMD, 0x11);  // CMD: accel normal mode
  delay(50);

  // ── Set gyroscope to normal mode ──
  writeReg(REG_CMD, 0x15);  // CMD: gyro normal mode
  delay(100);

  // ── Check power mode status ──
  uint8_t pmu = readReg(REG_PMU_STATUS);
  Serial.printf("PMU Status: 0x%02X\n", pmu);

  uint8_t accPmu  = (pmu >> 4) & 0x03;
  uint8_t gyrPmu  = (pmu >> 2) & 0x03;
  Serial.printf("  Accel mode: %s\n",
    accPmu == 0 ? "SUSPEND" :
    accPmu == 1 ? "NORMAL" :
    accPmu == 2 ? "LOW_POWER" : "UNKNOWN");
  Serial.printf("  Gyro mode:  %s\n",
    gyrPmu == 0 ? "SUSPEND" :
    gyrPmu == 1 ? "RESERVED" :
    gyrPmu == 2 ? "RESERVED" :
    gyrPmu == 3 ? "NORMAL" : "UNKNOWN");

  // ── Configure accel: 100Hz, +/- 2g ──
  writeReg(REG_ACC_CONF, 0x28);   // ODR=100Hz, BWP=normal
  writeReg(REG_ACC_RANGE, 0x03);  // +/- 2g (16384 LSB/g)

  // ── Configure gyro: 100Hz, +/- 250 deg/s ──
  writeReg(REG_GYR_CONF, 0x28);   // ODR=100Hz, BWP=normal
  writeReg(REG_GYR_RANGE, 0x03);  // +/- 250 deg/s (131 LSB/deg/s)

  // ── Check for errors ──
  uint8_t err = readReg(REG_ERR_REG);
  if (err != 0) {
    Serial.printf("WARNING: Error register = 0x%02X\n", err);
  } else {
    Serial.println("No errors.");
  }

  Serial.println("\nBMI160 ready!\n");
  Serial.println("AccX\tAccY\tAccZ\tGyroX\tGyroY\tGyroZ");
  Serial.println("----\t----\t----\t-----\t-----\t-----");
  Serial.println("(Board flat: AccZ ~ 16384, AccX/Y ~ 0)");
  Serial.println("(Tilt board to see AccX/AccY change)\n");
}

void loop() {
  readSensors();

  Serial.printf("%d\t%d\t%d\t%d\t%d\t%d\n",
    accX, accY, accZ, gyroX, gyroY, gyroZ);

  delay(100);
}
