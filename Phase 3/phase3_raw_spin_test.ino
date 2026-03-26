/*
 * Phase 3 — Raw Motor Spin Test (NO SimpleFOC)
 * 
 * Manually cycles through 6-step commutation.
 * If this spins the motor, SimpleFOC config is the issue.
 * If this doesn't spin it, wiring or motor is the issue.
 * 
 * Wiring:
 *   GPIO 4 → IN1, GPIO 5 → IN2, GPIO 6 → IN3
 *   GPIO 7 → EN
 *   12V on + pin
 * 
 * Author: Yogeshwaran
 */

#define IN1  4
#define IN2  5
#define IN3  6
#define EN   7

#define PWM_FREQ  20000
#define PWM_RES   8

// Voltage control (0-255 = 0-100% of 12V)
// 128 = ~6V, 170 = ~8V, 200 = ~9.4V, 220 = ~10.3V
int pwmDuty = 180;  // Start at ~8.5V

// Step delay in microseconds (controls speed)
// Bigger = slower, smaller = faster
int stepDelay = 3000;  // Start slow

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("=== Raw Motor Spin Test ===");
  Serial.println("No SimpleFOC — manual 6-step commutation\n");

  // Setup EN pin
  pinMode(EN, OUTPUT);
  digitalWrite(EN, HIGH);
  Serial.println("EN pin HIGH");

  // Setup PWM on all 3 phases
  ledcAttach(IN1, PWM_FREQ, PWM_RES);
  ledcAttach(IN2, PWM_FREQ, PWM_RES);
  ledcAttach(IN3, PWM_FREQ, PWM_RES);

  // All off
  ledcWrite(IN1, 0);
  ledcWrite(IN2, 0);
  ledcWrite(IN3, 0);

  Serial.println("PWM channels ready\n");
  Serial.printf("Duty: %d/255 (~%.1fV)\n", pwmDuty, pwmDuty / 255.0 * 12.0);
  Serial.printf("Step delay: %d us\n\n", stepDelay);

  Serial.println("Commands:");
  Serial.println("  s = start spinning");
  Serial.println("  0 = stop");
  Serial.println("  + = faster");
  Serial.println("  - = slower");
  Serial.println("  u = voltage up");
  Serial.println("  d = voltage down");
  Serial.println("  t = single step test (one phase at a time)");
  Serial.println();
}

// 6-step commutation table for BLDC
// Each step energizes 2 of 3 phases
void commStep(int step) {
  switch (step % 6) {
    case 0: ledcWrite(IN1, pwmDuty); ledcWrite(IN2, 0);       ledcWrite(IN3, 0);       break;
    case 1: ledcWrite(IN1, pwmDuty); ledcWrite(IN2, 0);       ledcWrite(IN3, pwmDuty); break;
    case 2: ledcWrite(IN1, 0);       ledcWrite(IN2, 0);       ledcWrite(IN3, pwmDuty); break;
    case 3: ledcWrite(IN1, 0);       ledcWrite(IN2, pwmDuty); ledcWrite(IN3, pwmDuty); break;
    case 4: ledcWrite(IN1, 0);       ledcWrite(IN2, pwmDuty); ledcWrite(IN3, 0);       break;
    case 5: ledcWrite(IN1, pwmDuty); ledcWrite(IN2, pwmDuty); ledcWrite(IN3, 0);       break;
  }
}

// Sinusoidal commutation (smoother for gimbal motors)
void sinStep(float angle) {
  float a = sin(angle);
  float b = sin(angle + 2.094);  // +120 degrees
  float c = sin(angle + 4.189);  // +240 degrees

  // Map -1..1 to 0..pwmDuty
  int da = (int)((a + 1.0) * 0.5 * pwmDuty);
  int db = (int)((b + 1.0) * 0.5 * pwmDuty);
  int dc = (int)((c + 1.0) * 0.5 * pwmDuty);

  ledcWrite(IN1, da);
  ledcWrite(IN2, db);
  ledcWrite(IN3, dc);
}

bool spinning = false;
bool useSine = true;  // Sine is better for gimbal motors
float angle = 0;

void allOff() {
  ledcWrite(IN1, 0);
  ledcWrite(IN2, 0);
  ledcWrite(IN3, 0);
}

void singleStepTest() {
  Serial.println("\n── Single Phase Test ──");
  Serial.println("Each phase gets voltage for 1 second.");
  Serial.println("Motor should twitch/hold in 3 different positions.\n");

  Serial.println("Phase A only...");
  ledcWrite(IN1, pwmDuty); ledcWrite(IN2, 0); ledcWrite(IN3, 0);
  delay(1000);

  Serial.println("Phase B only...");
  ledcWrite(IN1, 0); ledcWrite(IN2, pwmDuty); ledcWrite(IN3, 0);
  delay(1000);

  Serial.println("Phase C only...");
  ledcWrite(IN1, 0); ledcWrite(IN2, 0); ledcWrite(IN3, pwmDuty);
  delay(1000);

  allOff();
  Serial.println("Done. Did motor move to 3 positions?\n");
}

void loop() {
  // Check commands
  if (Serial.available()) {
    char cmd = Serial.read();

    switch (cmd) {
      case 's':
      case 'S':
        spinning = true;
        angle = 0;
        Serial.println("SPINNING (sine commutation)");
        break;

      case '0':
        spinning = false;
        allOff();
        Serial.println("STOPPED");
        break;

      case '+':
        stepDelay -= 500;
        if (stepDelay < 200) stepDelay = 200;
        Serial.printf("Faster: delay=%d us\n", stepDelay);
        break;

      case '-':
        stepDelay += 500;
        if (stepDelay > 10000) stepDelay = 10000;
        Serial.printf("Slower: delay=%d us\n", stepDelay);
        break;

      case 'u':
      case 'U':
        pwmDuty += 20;
        if (pwmDuty > 250) pwmDuty = 250;
        Serial.printf("Voltage up: %d/255 (~%.1fV)\n", pwmDuty, pwmDuty / 255.0 * 12.0);
        break;

      case 'd':
      case 'D':
        pwmDuty -= 20;
        if (pwmDuty < 20) pwmDuty = 20;
        Serial.printf("Voltage down: %d/255 (~%.1fV)\n", pwmDuty, pwmDuty / 255.0 * 12.0);
        break;

      case 't':
      case 'T':
        spinning = false;
        allOff();
        singleStepTest();
        break;
    }
  }

  // Spin loop
  if (spinning) {
    sinStep(angle);

    // Increment angle (one electrical revolution = 2*PI)
    // Divide by pole pairs for mechanical speed
    angle += 0.01;  // Small step for smooth rotation
    if (angle > 6.283) angle -= 6.283;

    delayMicroseconds(stepDelay);
  }
}
