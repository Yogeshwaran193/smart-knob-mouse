/*
 * Phase 3 — Step 1: PWM Signal Test (NO MOTOR SPIN)
 * 
 * Outputs PWM on the 3 motor pins one at a time.
 * Use a multimeter to verify signals reach the DR10058 driver.
 * 
 * DO NOT connect 12V yet for this test.
 * We're just checking that PWM signals get from ESP32 to the driver.
 * 
 * What to measure with multimeter (DC voltage mode):
 *   - At 50% duty: ~1.65V average on the ESP32 GPIO pin
 *   - Same voltage should appear at the DR10058 input pin
 *   - Each phase ramps 0% → 100% → 0% one at a time
 * 
 * Wiring:
 *   GPIO 4 → DR10058 PWM_A (Phase A)
 *   GPIO 5 → DR10058 PWM_B (Phase B)
 *   GPIO 6 → DR10058 PWM_C (Phase C)
 * 
 * Author: Yogeshwaran
 */

#define PWM_A  4
#define PWM_B  5
#define PWM_C  6

// PWM config
#define PWM_FREQ   20000  // 20kHz — above audible range
#define PWM_RES    8      // 8-bit: 0-255

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("=== Phase 3 Step 1: PWM Signal Test ===");
  Serial.println("DO NOT connect 12V yet!");
  Serial.println("Use multimeter on each GPIO pin.\n");

  // Setup PWM channels
  ledcAttach(PWM_A, PWM_FREQ, PWM_RES);
  ledcAttach(PWM_B, PWM_FREQ, PWM_RES);
  ledcAttach(PWM_C, PWM_FREQ, PWM_RES);

  // Start all at 0
  ledcWrite(PWM_A, 0);
  ledcWrite(PWM_B, 0);
  ledcWrite(PWM_C, 0);

  Serial.println("PWM channels ready on GPIO 4, 5, 6\n");
}

void testPhase(const char* name, uint8_t pin) {
  Serial.printf("── Testing %s (GPIO %d) ──\n", name, pin);

  // Ramp up
  Serial.println("  Ramping UP: 0%% → 100%%");
  for (int duty = 0; duty <= 255; duty += 5) {
    ledcWrite(pin, duty);
    float voltage = (duty / 255.0) * 3.3;
    if (duty % 50 == 0) {
      Serial.printf("  Duty: %3d/255 (%3d%%)  Expected: %.2fV\n",
        duty, (duty * 100) / 255, voltage);
    }
    delay(30);
  }

  delay(500);

  // Ramp down
  Serial.println("  Ramping DOWN: 100%% → 0%%");
  for (int duty = 255; duty >= 0; duty -= 5) {
    ledcWrite(pin, duty);
    delay(30);
  }

  ledcWrite(pin, 0);
  Serial.printf("  %s done.\n\n", name);
  delay(500);
}

void loop() {
  // Test each phase one at a time
  testPhase("Phase A", PWM_A);
  testPhase("Phase B", PWM_B);
  testPhase("Phase C", PWM_C);

  // Hold at 50% on all three for multimeter measurement
  Serial.println("── All phases at 50%% (hold for measurement) ──");
  Serial.println("  Expected: ~1.65V on each GPIO pin");
  Serial.println("  Measure at DR10058 input pins too.\n");
  ledcWrite(PWM_A, 128);
  ledcWrite(PWM_B, 128);
  ledcWrite(PWM_C, 128);

  delay(5000);

  // All off
  ledcWrite(PWM_A, 0);
  ledcWrite(PWM_B, 0);
  ledcWrite(PWM_C, 0);
  Serial.println("── All off. Restarting in 3 seconds... ──\n");
  delay(3000);
}
