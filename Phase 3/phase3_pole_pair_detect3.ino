/*
 * Phase 3 — Pole Pair Detection (Raw Sinusoidal, No SimpleFOC)
 * 
 * Uses the same sinusoidal commutation that worked in the spin test
 * plus AS5600 encoder to measure how far the shaft moves.
 * 
 * Wiring:
 *   GPIO 4 → IN1, GPIO 5 → IN2, GPIO 6 → IN3
 *   GPIO 7 → EN
 *   AS5600: SDA=8, SCL=9
 *   12V on + pin
 * 
 * Send 's' in Serial Monitor to start.
 * Don't touch the motor after pressing 's'.
 * 
 * Author: Yogeshwaran
 */

#include <Wire.h>

// ── Pins ──
#define IN1  4
#define IN2  5
#define IN3  6
#define EN   7

#define PWM_FREQ  20000
#define PWM_RES   8

// AS5600
#define AS5600_SDA   8
#define AS5600_SCL   9
#define AS5600_ADDR  0x36

int pwmDuty = 180;  // ~8.5V

// ── AS5600 read angle (returns 0-4095) ──
uint16_t readAngle() {
  Wire.beginTransmission(AS5600_ADDR);
  Wire.write(0x0C);
  Wire.endTransmission(false);
  Wire.requestFrom(AS5600_ADDR, (uint8_t)2);
  uint8_t high = Wire.read();
  uint8_t low = Wire.read();
  return ((high & 0x0F) << 8) | low;
}

// ── Convert raw to degrees (handles wraparound) ──
float rawToDeg(uint16_t raw) {
  return (raw / 4096.0) * 360.0;
}

// ── Calculate angle delta with wraparound handling ──
float angleDelta(uint16_t current, uint16_t previous) {
  int16_t delta = (int16_t)current - (int16_t)previous;
  if (delta > 2048) delta -= 4096;
  if (delta < -2048) delta += 4096;
  return (delta / 4096.0) * 360.0;
}

// ── Sinusoidal commutation (same as raw spin test) ──
void sinDrive(float elecAngle) {
  float a = sin(elecAngle);
  float b = sin(elecAngle + 2.094);  // +120 deg
  float c = sin(elecAngle + 4.189);  // +240 deg

  int da = (int)((a + 1.0) * 0.5 * pwmDuty);
  int db = (int)((b + 1.0) * 0.5 * pwmDuty);
  int dc = (int)((c + 1.0) * 0.5 * pwmDuty);

  ledcWrite(IN1, da);
  ledcWrite(IN2, db);
  ledcWrite(IN3, dc);
}

void allOff() {
  ledcWrite(IN1, 0);
  ledcWrite(IN2, 0);
  ledcWrite(IN3, 0);
}

// ── Drive through N electrical revolutions, return mechanical degrees moved ──
float driveElecRevs(int numRevs, uint16_t &startRaw) {
  int stepsPerRev = 500;
  float totalMechDeg = 0;
  uint16_t prevRaw = startRaw;

  for (int rev = 0; rev < numRevs; rev++) {
    for (int i = 0; i <= stepsPerRev; i++) {
      float elecAngle = (rev * 2.0 * PI) + ((float)i / stepsPerRev * 2.0 * PI);
      sinDrive(elecAngle);
      delayMicroseconds(1500);
    }

    // Read encoder after each revolution
    uint16_t nowRaw = readAngle();
    float delta = angleDelta(nowRaw, prevRaw);
    totalMechDeg += delta;
    prevRaw = nowRaw;

    Serial.printf("  Elec rev %d: shaft at %.1f deg (moved %.1f deg this rev)\n",
      rev + 1, rawToDeg(nowRaw), delta);
  }

  startRaw = prevRaw;
  return totalMechDeg;
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("=== Phase 3: Pole Pair Detection ===");
  Serial.println("Raw sinusoidal + AS5600 (no SimpleFOC)\n");

  // Init I2C for AS5600
  Wire.begin(AS5600_SDA, AS5600_SCL);
  Wire.setClock(400000);

  // Check AS5600
  Wire.beginTransmission(AS5600_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("ERROR: AS5600 not found at 0x36!");
    while (1) delay(1000);
  }
  Serial.println("AS5600 found at 0x36");
  Serial.printf("Current angle: %.1f deg\n\n", rawToDeg(readAngle()));

  // Init motor pins
  pinMode(EN, OUTPUT);
  digitalWrite(EN, HIGH);
  ledcAttach(IN1, PWM_FREQ, PWM_RES);
  ledcAttach(IN2, PWM_FREQ, PWM_RES);
  ledcAttach(IN3, PWM_FREQ, PWM_RES);
  allOff();

  Serial.println("Motor driver ready.");
  Serial.printf("PWM duty: %d/255 (~%.1fV)\n\n", pwmDuty, pwmDuty / 255.0 * 12.0);
  Serial.println("Send 's' to start. Don't touch the motor after!\n");

  // Wait for start
  while (true) {
    if (Serial.available()) {
      char cmd = Serial.read();
      if (cmd == 's' || cmd == 'S') break;
    }
    delay(10);
  }

  Serial.println("Starting pole pair detection...\n");

  // ═══════════════════════════════════════════
  //  TEST 1: Drive 1 electrical revolution
  // ═══════════════════════════════════════════
  Serial.println("── Test 1: 1 electrical revolution ──");
  uint16_t startRaw = readAngle();
  float mechDeg1 = driveElecRevs(1, startRaw);
  float pp1 = 360.0 / abs(mechDeg1);
  Serial.printf("  Shaft moved: %.1f mechanical degrees\n", mechDeg1);
  Serial.printf("  Estimated PP: %.1f\n\n", pp1);

  allOff();
  delay(500);

  // ═══════════════════════════════════════════
  //  TEST 2: Drive 7 electrical revolutions
  //  (If PP=7, this = exactly 1 full mech turn)
  // ═══════════════════════════════════════════
  Serial.println("── Test 2: 7 electrical revolutions ──");
  startRaw = readAngle();
  float mechDeg7 = driveElecRevs(7, startRaw);
  float pp7 = 7.0 * 360.0 / abs(mechDeg7);
  Serial.printf("  Shaft moved: %.1f mechanical degrees total\n", mechDeg7);
  Serial.printf("  Estimated PP: %.1f\n\n", pp7);

  allOff();

  // ═══════════════════════════════════════════
  //  RESULT
  // ═══════════════════════════════════════════
  float avgPP = (pp1 + pp7) / 2.0;
  int detectedPP = round(avgPP);

  Serial.println("════════════════════════════════════");
  Serial.printf("  Test 1 estimate: %.1f pole pairs\n", pp1);
  Serial.printf("  Test 2 estimate: %.1f pole pairs\n", pp7);
  Serial.printf("  Average:         %.1f\n", avgPP);
  Serial.println();
  Serial.printf("  DETECTED POLE PAIRS: %d\n", detectedPP);

  if (detectedPP == 7) {
    Serial.println("  CONFIRMED: 7 pole pairs!");
    Serial.println("  Matches 2204 gimbal motor (12N14P)");
  } else {
    Serial.printf("  UNEXPECTED! Expected 7, got %d\n", detectedPP);
    Serial.println("  Re-run test or check motor spec");
  }
  Serial.println("════════════════════════════════════");
  Serial.println("\nPhase 3 complete! Ready for Phase 4.");

  // Disable motor
  digitalWrite(EN, LOW);
}

void loop() {
  delay(1000);
}
