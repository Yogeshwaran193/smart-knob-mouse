/*
 * IMU Diagnostic — Identify your chip
 * 
 * Reads key registers to figure out what IMU chip
 * is actually on your breakout board.
 * 
 * Wiring:
 *   SDA → GPIO 37,  SCL → GPIO 38
 *   3V3 → 3.3V,     GND → GND
 * 
 * Author: Yogeshwaran
 */

#include <Wire.h>

#define IMU_SDA  37
#define IMU_SCL  38

uint8_t imuAddr = 0;

uint8_t readReg(uint8_t reg) {
  Wire1.beginTransmission(imuAddr);
  Wire1.write(reg);
  Wire1.endTransmission(false);
  Wire1.requestFrom(imuAddr, (uint8_t)1);
  if (Wire1.available()) return Wire1.read();
  return 0xFF;
}

void writeReg(uint8_t reg, uint8_t val) {
  Wire1.beginTransmission(imuAddr);
  Wire1.write(reg);
  Wire1.write(val);
  Wire1.endTransmission();
}

void dumpRegisters() {
  Serial.println("\n--- Key Register Dump ---");
  
  uint8_t regs[] = {0x00, 0x01, 0x02, 0x6A, 0x6B, 0x6C, 0x75, 0x70, 0x71, 0x72};
  const char* names[] = {
    "0x00 (config/ID?)", "0x01", "0x02", 
    "0x6A (USER_CTRL)", "0x6B (PWR_MGMT_1)", "0x6C (PWR_MGMT_2)",
    "0x75 (WHO_AM_I)", "0x70", "0x71", "0x72"
  };
  
  for (int i = 0; i < 10; i++) {
    uint8_t val = readReg(regs[i]);
    Serial.printf("  Reg 0x%02X = 0x%02X  (%s)\n", regs[i], val, names[i]);
  }
}

void identifyChip(uint8_t whoami) {
  Serial.printf("\nWHO_AM_I value: 0x%02X → ", whoami);
  
  switch (whoami) {
    case 0x68: Serial.println("MPU6050 (genuine)"); break;
    case 0x70: Serial.println("MPU6500"); break;
    case 0x71: Serial.println("MPU9250"); break;
    case 0x73: Serial.println("MPU9255"); break;
    case 0x19: Serial.println("MPU6886 (M5Stack)"); break;
    case 0x98: Serial.println("ICM-20689"); break;
    case 0x12: Serial.println("ICM-20602"); break;
    case 0x11: Serial.println("ICM-20600"); break;
    case 0xAC: Serial.println("ICM-20648"); break;
    case 0xAF: Serial.println("ICM-20649"); break;
    case 0x05: Serial.println("QMI8658 (common clone)"); break;
    case 0x00: Serial.println("Unknown — likely clone or wiring issue"); break;
    case 0xFF: Serial.println("No response — check wiring"); break;
    default:   Serial.println("Unknown chip"); break;
  }
}

void readAndPrint(const char* label) {
  Wire1.beginTransmission(imuAddr);
  Wire1.write(0x3B);
  Wire1.endTransmission(false);
  Wire1.requestFrom(imuAddr, (uint8_t)14);

  int16_t ax = (Wire1.read() << 8) | Wire1.read();
  int16_t ay = (Wire1.read() << 8) | Wire1.read();
  int16_t az = (Wire1.read() << 8) | Wire1.read();
  int16_t t  = (Wire1.read() << 8) | Wire1.read();
  int16_t gx = (Wire1.read() << 8) | Wire1.read();
  int16_t gy = (Wire1.read() << 8) | Wire1.read();
  int16_t gz = (Wire1.read() << 8) | Wire1.read();

  Serial.printf("  %s: AX=%d AY=%d AZ=%d GX=%d GY=%d GZ=%d\n",
    label, ax, ay, az, gx, gy, gz);
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("=== IMU Diagnostic Tool ===\n");

  // ── Try normal wiring first ──
  Serial.println(">> Testing SDA=37, SCL=38");
  Wire1.begin(IMU_SDA, IMU_SCL);
  Wire1.setClock(100000);  // Start slow for reliability

  // Scan
  Serial.println("\n--- I2C Scan ---");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire1.beginTransmission(addr);
    if (Wire1.endTransmission() == 0) {
      Serial.printf("  Device at 0x%02X\n", addr);
      if (addr == 0x68 || addr == 0x69) imuAddr = addr;
      found++;
    }
  }
  
  if (found == 0) {
    // ── Try swapped wiring ──
    Serial.println("\n  Nothing found! Trying SWAPPED pins (SDA=38, SCL=37)...");
    Wire1.end();
    Wire1.begin(IMU_SCL, IMU_SDA);  // Swap!
    Wire1.setClock(100000);
    
    for (uint8_t addr = 1; addr < 127; addr++) {
      Wire1.beginTransmission(addr);
      if (Wire1.endTransmission() == 0) {
        Serial.printf("  Device at 0x%02X (with SWAPPED pins!)\n", addr);
        Serial.println("  >>> Your SDA and SCL wires are swapped! <<<");
        if (addr == 0x68 || addr == 0x69) imuAddr = addr;
        found++;
      }
    }
  }

  if (imuAddr == 0) {
    Serial.println("\nNo IMU found on either pin config. Check wiring.");
    while (1) delay(1000);
  }

  Serial.printf("\nUsing IMU at 0x%02X\n", imuAddr);

  // ── Read WHO_AM_I before reset ──
  uint8_t whoami_before = readReg(0x75);
  identifyChip(whoami_before);

  // ── Dump registers before reset ──
  Serial.println("\n=== BEFORE RESET ===");
  dumpRegisters();
  readAndPrint("Data before reset");

  // ── Hard reset ──
  Serial.println("\n=== RESETTING CHIP ===");
  writeReg(0x6B, 0x80);  // Device reset bit
  delay(200);

  // Wait for reset to complete
  for (int i = 0; i < 10; i++) {
    uint8_t pwr = readReg(0x6B);
    Serial.printf("  PWR_MGMT_1 after reset: 0x%02X\n", pwr);
    if ((pwr & 0x80) == 0) break;  // Reset bit cleared
    delay(50);
  }

  // ── Wake up ──
  writeReg(0x6B, 0x01);  // Clock source: PLL with X gyro
  delay(100);
  writeReg(0x1A, 0x03);  // DLPF: 44Hz
  writeReg(0x1B, 0x00);  // Gyro: +/-250 deg/s
  writeReg(0x1C, 0x00);  // Accel: +/-2g
  delay(100);

  // ── Read WHO_AM_I after reset ──
  uint8_t whoami_after = readReg(0x75);
  Serial.printf("\nWHO_AM_I after reset: 0x%02X\n", whoami_after);
  identifyChip(whoami_after);

  // ── Dump registers after reset ──
  Serial.println("\n=== AFTER RESET ===");
  dumpRegisters();
  readAndPrint("Data after reset");

  // ── Continuous reading test ──
  Serial.println("\n=== LIVE DATA (tilt the board!) ===");
  Serial.println("AccX\tAccY\tAccZ\tGyroX\tGyroY\tGyroZ");
  Serial.println("----\t----\t----\t-----\t-----\t-----");
}

void loop() {
  Wire1.beginTransmission(imuAddr);
  Wire1.write(0x3B);
  Wire1.endTransmission(false);
  Wire1.requestFrom(imuAddr, (uint8_t)14);

  int16_t ax = (Wire1.read() << 8) | Wire1.read();
  int16_t ay = (Wire1.read() << 8) | Wire1.read();
  int16_t az = (Wire1.read() << 8) | Wire1.read();
  Wire1.read(); Wire1.read();  // Skip temp
  int16_t gx = (Wire1.read() << 8) | Wire1.read();
  int16_t gy = (Wire1.read() << 8) | Wire1.read();
  int16_t gz = (Wire1.read() << 8) | Wire1.read();

  Serial.printf("%d\t%d\t%d\t%d\t%d\t%d\n",
    ax, ay, az, gx, gy, gz);

  delay(200);
}
