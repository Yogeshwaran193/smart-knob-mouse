/*
 * Phase 2 — AS5600 Hall Effect Encoder Test
 * 
 * Reads angle from AS5600 over I2C, shows:
 *   - Raw angle (0-4095)
 *   - Degrees (0-360)
 *   - Rotation direction (CW / CCW / STILL)
 *   - Magnet status (detected, too close, too far)
 *   - RPM estimate
 * 
 * Wiring:
 *   SDA → GPIO 8,  SCL → GPIO 9
 *   VCC → 3.3V,    GND → GND
 *   DIR → GND (CW = increasing angle)
 * 
 * AS5600 I2C address: 0x36
 * 
 * Author: Yogeshwaran
 */

#include <Wire.h>

#define AS5600_SDA   8
#define AS5600_SCL   9
#define AS5600_ADDR  0x36

// ── AS5600 Registers ──
#define REG_RAW_ANGLE_H  0x0C  // Raw angle (12-bit, 2 bytes)
#define REG_RAW_ANGLE_L  0x0D
#define REG_ANGLE_H      0x0E  // Filtered angle
#define REG_ANGLE_L      0x0F
#define REG_STATUS       0x0B  // Magnet status
#define REG_AGC          0x1A  // Auto gain control
#define REG_MAGNITUDE_H  0x1B  // Magnet magnitude

// ── State ──
int16_t prevAngle = 0;
int32_t totalRotation = 0;  // Tracks cumulative rotation
unsigned long prevTime = 0;

// ── Read 2-byte register ──
uint16_t readAngle() {
  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(REG_RAW_ANGLE_H);
  Wire.endTransmission(false);
  Wire.requestFrom(AS5600_ADDR, (uint8_t)2);

  uint8_t high = Wire.read();
  uint8_t low = Wire.read();

  return ((high & 0x0F) << 8) | low;  // 12-bit value: 0-4095
}

uint8_t readReg(uint8_t reg) {
  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(AS5600_ADDR, (uint8_t)1);
  return Wire.read();
}

// ── Check magnet status ──
void checkMagnet() {
  uint8_t status = readReg(REG_STATUS);
  uint8_t agc = readReg(REG_AGC);

  bool detected = (status & 0x20) != 0;  // MD bit
  bool tooWeak  = (status & 0x10) != 0;  // ML bit
  bool tooStrong = (status & 0x08) != 0; // MH bit

  Serial.printf("Magnet status: 0x%02X\n", status);
  Serial.printf("  Detected:   %s\n", detected ? "YES" : "NO ← need magnet!");
  Serial.printf("  Too weak:   %s\n", tooWeak ? "YES ← move magnet closer" : "NO");
  Serial.printf("  Too strong: %s\n", tooStrong ? "YES ← move magnet farther" : "NO");
  Serial.printf("  AGC value:  %d (ideal: 40-200)\n", agc);

  if (!detected) {
    Serial.println("\n*** Place a diametrically magnetized magnet ***");
    Serial.println("*** directly above the AS5600 chip (1-2mm) ***\n");
  }
}

// ── Calculate rotation delta (handles wraparound) ──
int16_t calcDelta(int16_t current, int16_t previous) {
  int16_t delta = current - previous;

  // Handle wraparound: if delta is huge, we wrapped past 0/4095
  if (delta > 2048) delta -= 4096;       // Wrapped CCW past 0
  if (delta < -2048) delta += 4096;      // Wrapped CW past 4095

  return delta;
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("=== Phase 2: AS5600 Encoder Test ===\n");

  Wire.begin(AS5600_SDA, AS5600_SCL);
  Wire.setClock(400000);

  // Verify chip is there
  Wire.beginTransmission(AS5600_ADDR);
  uint8_t err = Wire.endTransmission();
  if (err != 0) {
    Serial.println("ERROR: AS5600 not found at 0x36!");
    Serial.println("Check SDA=GPIO8, SCL=GPIO9, power, GND");
    while (1) delay(1000);
  }
  Serial.println("AS5600 found at 0x36\n");

  // Check magnet
  checkMagnet();

  // Read initial angle
  prevAngle = readAngle();
  prevTime = millis();

  Serial.println("\nRotate the magnet/shaft to test!\n");
  Serial.println("Raw\tDeg\tDir\tRPM\tTotal_Deg");
  Serial.println("---\t---\t---\t---\t---------");
}

void loop() {
  uint16_t raw = readAngle();
  float degrees = (raw / 4096.0) * 360.0;

  // Calculate delta
  int16_t delta = calcDelta(raw, prevAngle);
  totalRotation += delta;

  // Direction
  const char* dir;
  if (delta > 5) dir = "CW  >>>";
  else if (delta < -5) dir = "CCW <<<";
  else dir = "STILL  ";

  // RPM estimate
  unsigned long now = millis();
  float dt = (now - prevTime) / 1000.0;  // seconds
  float rpm = 0;
  if (dt > 0) {
    float degsPerSec = (delta / 4096.0) * 360.0 / dt;
    rpm = degsPerSec / 6.0;  // 360 deg/s = 60 RPM
  }

  // Total rotation in degrees
  float totalDeg = (totalRotation / 4096.0) * 360.0;

  // Print
  Serial.printf("%d\t%.1f\t%s\t%.1f\t%.1f\n",
    raw, degrees, dir, rpm, totalDeg);

  prevAngle = raw;
  prevTime = now;

  delay(50);  // 20Hz update
}
