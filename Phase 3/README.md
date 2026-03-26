# Phase 3: Motor & Driver Validation (Open-Loop)

**Goal:** Verify the SimpleFOC Mini (MS8313) driver and 2204 gimbal motor work correctly. Test in three stages: PWM signals without power, open-loop motor spin, and pole pair detection with the AS5600 encoder.

**Hardware needed:** Varies per test (see table below)

**Status:** ✅ Complete

## Hardware Setup

| Component | Details |
|-----------|---------|
| Motor Driver | SimpleFOC Mini v1.0 (MPS MS8313 3-phase driver) |
| Motor | 2204 Gimbal (12N14P, 7 pole pairs, KV260) |
| Encoder | AS5600 on Wire1 (SDA=8, SCL=9) — needed for pole pair test only |
| Boost Converter | 5V → 12V — needed for spin test and pole pair test |
| PWM Pins | GPIO 4 (Phase A), GPIO 5 (Phase B), GPIO 6 (Phase C) |

## Files (run in order)

| File | Purpose | Hardware Needed | 12V Required? |
|------|---------|-----------------|---------------|
| `phase3_pwm_signal_test.ino` | Verify PWM signals reach the driver inputs | ESP32-S3 + SimpleFOC Mini (no motor needed) | **No** |
| `phase3_raw_spin_test.ino` | Open-loop motor spin — confirm motor and driver work | ESP32-S3 + SimpleFOC Mini + motor + 12V | **Yes** |
| `phase3_pole_pair_detect3.ino` | Detect motor pole pairs using AS5600 feedback | Full circuit (ESP32 + driver + motor + AS5600 + 12V) | **Yes** |

## Test 1: PWM Signal Test (`phase3_pwm_signal_test.ino`)

This test can be run **without the boost converter** — it only checks that the ESP32's PWM outputs reach the driver's input pins.

**What it does:** Ramps PWM duty cycle on each phase (A, B, C) one at a time. You measure the voltage at the ESP32 GPIO pin and at the corresponding SimpleFOC Mini input pin with a multimeter.

**How to verify:**
- At 50% duty cycle, the multimeter should read ~1.65V DC average on each GPIO
- The same voltage should appear at the SimpleFOC Mini input pins (confirming the signal reaches the driver)
- Ramp from 0–100% should show 0V → 3.3V smoothly

**Pass criteria:** All three phases show correct PWM voltage at both the ESP32 GPIO and the driver input.

## Test 2: Motor Spin Test (`phase3_raw_spin_test.ino`)

**⚠️ Requires 12V from the boost converter.**

**What it does:** Runs SimpleFOC in open-loop velocity mode. The motor should spin smoothly and continuously.

**What to look for:**
- Motor rotates smoothly in one direction — this confirms the driver is commutating correctly
- If the motor vibrates but doesn't spin, the phase wiring order may be wrong — try swapping any two of the three motor wires
- If the motor does nothing, check that 12V is reaching the SimpleFOC Mini's VMOT pin

**Pass criteria:** Motor spins smoothly in open-loop mode with no vibration or stalling.

## Test 3: Pole Pair Detection (`phase3_pole_pair_detect3.ino`)

**⚠️ Requires 12V + AS5600 connected. Do NOT touch the motor shaft during this test.**

**What it does:** Uses SimpleFOC's sensor alignment routine to determine the number of pole pairs by slowly driving the motor through electrical cycles while reading the AS5600 angle. The motor will rotate slowly on its own during this process.

**Expected result:** The serial output should report **7 pole pairs**, matching the 2204 motor's 12N14P (14 magnets ÷ 2 = 7) configuration.

**What 7 pole pairs means:**
- 1 mechanical revolution = 7 electrical cycles
- Each electrical cycle spans 360° ÷ 7 = 51.43° mechanical
- The AS5600 provides 4096 ÷ 7 ≈ 585 counts per electrical cycle (well above SimpleFOC's minimum of ~50–100)

**Pass criteria:** Pole pair count reported as 7. If it reports a different number, recount the magnets on the rotor (should be 14) and check AS5600 magnet alignment.

## Wiring Summary

```
ESP32-S3                    SimpleFOC Mini (MS8313)
─────────                   ───────────────────────
GPIO 4  ───── PWM A ─────▶  IN1
GPIO 5  ───── PWM B ─────▶  IN2
GPIO 6  ───── PWM C ─────▶  IN3
3V3     ─────────────────▶  VCC (logic supply)
GND     ─────────────────▶  GND

Boost Converter (12V out) ─▶  VMOT (motor power)

SimpleFOC Mini ──── 3 phase wires ────▶ 2204 Gimbal Motor

AS5600 (for pole pair test only)
──────
SDA ──▶ GPIO 8  (Wire1)
SCL ──▶ GPIO 9  (Wire1)
VCC ──▶ 3V3
GND ──▶ GND
```

## Troubleshooting

- **No PWM signal at driver input** — Check wiring between ESP32 GPIOs and SimpleFOC Mini INx pins. Verify with multimeter at both ends.
- **Motor vibrates but won't spin** — Phase wiring is likely wrong. Swap any two of the three motor wires and try again.
- **Motor spins but is rough/jerky** — Normal for open-loop. Closed-loop FOC (Phase 4) will be much smoother.
- **Pole pair count ≠ 7** — Check AS5600 magnet alignment. Make sure the magnet is diametrically magnetized, centered, and 1–2mm above the IC. Also ensure the motor is free to rotate during the test.
- **Motor doesn't move at all** — Verify 12V is present on VMOT. Check that the enable pin on the SimpleFOC Mini isn't being held low.

## Arduino IDE Settings

- **Board:** ESP32S3 Dev Module
- **USB CDC On Boot:** Enabled (required for serial output on ESP32-S3)
- **Upload:** Hold BOOT button during upload if needed
- **Library:** SimpleFOC v2.3+ (install via Library Manager)
