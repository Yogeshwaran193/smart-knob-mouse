# Phase 2: AS5600 Hall Effect Sensor Communication

**Goal:** Verify I2C communication with the AS5600 magnetic encoder on Wire1 (I2C Bus 0) and confirm smooth angle tracking with a manually rotated magnet.

**Hardware needed:** ESP32-S3 + AS5600 breakout + diametrically magnetized magnet (no motor or boost converter required)

**Status:** ✅ Complete

## Wiring

| AS5600 Pin | ESP32-S3 GPIO | Notes |
|------------|---------------|-------|
| SDA | 8 | I2C Bus 0 (Wire1) |
| SCL | 9 | I2C Bus 0 (Wire1) |
| VCC | 3V3 | 3.3V from ESP32 regulator |
| GND | GND | Common ground |
| DIR | 3 | Rotation direction (optional) |

The AS5600 requires a **diametrically magnetized** magnet centered directly above the IC, with a 1–2mm air gap. Axially magnetized magnets will not work. Misalignment causes noisy or stuck readings.

## Files

| File | Purpose | How to Run |
|------|---------|------------|
| `phase2_as5600_test.ino` | I2C scan on Wire1, magnet status check, angle streaming | Upload → Serial Monitor (115200 baud) |

## What the Test Does

The sketch runs through a sequence of checks:

1. **I2C bus scan on Wire1** — confirms the AS5600 is detected at address `0x36`
2. **Magnet status register (0x0B)** — reads the MD (magnet detected), ML (too weak), and MH (too strong) flags
3. **AGC register (0x1A)** — checks the automatic gain control value is in a healthy range (40–200, not pegged at 0 or 255)
4. **Continuous angle output** — streams the raw 12-bit angle (0–4095) and converted degrees (0°–360°) at ~20Hz so you can rotate the magnet by hand and watch it sweep

## Pass Criteria

| # | Test | Expected Result |
|---|------|-----------------|
| 1 | I2C scan on Wire1 | Device found at `0x36` |
| 2 | Magnet status | MD=1 (detected), ML=0, MH=0 |
| 3 | AGC value | Between 40–200 (not 0 or 255) |
| 4 | Rotate magnet 360° | Angle sweeps 0° → 360° smoothly, no jumps |
| 5 | Return to start | Reads ~0° (within ±1°) |
| 6 | Hold magnet still | Noise < ±2 counts (< ±0.18°) |
| 7 | Vary magnet height (1–3mm) | Angle stays stable, AGC adjusts |

## Sensor Details

The AS5600 provides 12-bit absolute angle measurement — 4096 discrete counts per full revolution. That gives a mechanical resolution of 0.088° per count. For the 2204 motor with 7 pole pairs, this yields ~585 counts per electrical cycle, which is well above SimpleFOC's minimum requirement of ~50–100 counts per electrical cycle.

## Troubleshooting

- **"No device found at 0x36"** — Check that SDA/SCL aren't swapped, 3.3V is connected, and you're using `Wire1.begin(8, 9)` not `Wire.begin()`
- **MD=0 (no magnet detected)** — Magnet is too far away, wrong type (needs diametrically magnetized), or not centered over the IC
- **ML=1 (magnet too weak)** — Move magnet closer or use a stronger magnet
- **MH=1 (magnet too strong)** — Increase the air gap
- **AGC pegged at 0 or 255** — Magnet distance is way off, or VCC is unstable
- **Angle jumps or noise > ±5 counts** — Magnet not centered, or electrical interference from nearby wires
