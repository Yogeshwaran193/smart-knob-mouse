# Smart Knob Mouse

**A haptic scroll wheel powered by a BLDC gimbal motor under Field-Oriented Control (FOC)**

This project replaces the traditional scroll wheel on a computer mouse with a software-defined haptic knob. A brushless gimbal motor under closed-loop torque control provides virtual detents, endstops, spring-return behavior, and notification buzzes — all configurable in firmware with no mechanical changes.

Inspired by [Scott Bezek's SmartKnob](https://github.com/scottbez1/smartknob), adapted into a mouse form factor with IMU-based cursor control and BLE wireless connectivity.

> **Status:** Active development — Phases 1, 2, 3, 5a, and 5b complete. Currently working toward closed-loop FOC (Phase 4) and full mode integration.

---

## How It Works

The ESP32-S3 reads a 12-bit magnetic encoder (AS5600) on the motor shaft at high speed and uses the SimpleFOC library to apply precisely controlled torque via a 3-phase driver (SimpleFOC Mini / MS8313). The torque profile changes based on the active mode — sinusoidal detents for scrolling, linear springs for zoom, hard walls for volume limits, or zero torque for fidget spinning. A BMI160 IMU handles cursor movement. Everything communicates with the laptop over Bluetooth Low Energy.

```
┌─────────────────────────────────────────────────────────────────┐
│                        LAPTOP (Host)                            │
│   Python app: hotkey listener + notification bridge             │
│   Hotkeys: Ctrl+Alt+S/V/Z/F/C → mode switch via BLE           │
└──────────────────────────┬──────────────────────────────────────┘
                           │ BLE (wireless)
┌──────────────────────────▼──────────────────────────────────────┐
│                      ESP32-S3 (MCU)                             │
│  Core 0: FOC loop (~10kHz) + mode torque logic                 │
│  Core 1: IMU processing + BLE GATT server                      │
├─────────────┬───────────────┬───────────────┬───────────────────┤
│  BMI160     │   AS5600      │  SimpleFOC    │   USB Power Bank  │
│  (cursor)   │   (angle)     │  Mini+Motor   │   5V → ESP32      │
│  I2C @0x69  │   I2C @0x36   │   3x PWM      │   5V → 12V boost  │
│  Wire       │   Wire1       │   → 2204      │   → driver VMOT   │
└─────────────┴───────────────┴───────────────┴───────────────────┘
```

---

## Knob Modes

| # | Mode | Haptic Feel | Output to Laptop |
|---|------|-------------|-----------------|
| 1 | **Scroll** | Light virtual detents, infinite rotation | Mouse scroll events |
| 2 | **Volume** | Free-wheel between hard endstops (0°–100°) | Volume up/down media keys |
| 3 | **Zoom** | Spring-return to center, resistance ∝ displacement | Ctrl+scroll (zoom) |
| 4 | **Freewheel** | Zero torque, free spin | None (fidget mode) |
| 5 | **Click** | Hard snap detent, ±1 detent from center | L/R click, double-click, long-press |
| 6 | **Notification** | Sharp motor jerk impulse | None (triggered by laptop) |

Mode switching is done from the laptop via keyboard hotkeys (Ctrl+Alt+S/V/Z/F/C) sent over BLE — no physical button on the mouse.

---

## Hardware

| Component | Model | Role |
|-----------|-------|------|
| Microcontroller | ESP32-S3 | Dual-core 240MHz. FOC loop, BLE, sensor reads |
| IMU | BMI160 (labeled as MPU6050) | 6-axis accel+gyro for cursor control. I2C Bus 1 (Wire) |
| Magnetic Encoder | AS5600 | 12-bit absolute angle (4096 counts/rev). I2C Bus 0 (Wire1) |
| Motor Driver | SimpleFOC Mini v1.0 (MS8313) | 3-phase BLDC driver. Driven by 20kHz PWM from ESP32 |
| Gimbal Motor | 2204 (7 pole pairs) | 12N14P, KV260. Provides haptic torque feedback |
| Boost Converter | 5V → 12V | Powers motor driver. Isolates motor from digital logic |
| Power Source | USB Power Bank | 5V supply. Electrically isolates motor circuit from laptop |

> **IMU Note:** The breakout board is labeled MPU6050 but actually contains a **BMI160** (Bosch). Chip ID register 0x00 returns 0xD1. The BMI160 has a completely different register map — LSB-first data, CMD register for power modes, accel data at 0x12, gyro at 0x0C. The Phase 1 firmware is written for the BMI160.

### Pin Allocation (ESP32-S3)

| Function | GPIO | Bus/Peripheral |
|----------|------|----------------|
| IMU SDA | 37 | Wire (I2C Bus 1) |
| IMU SCL | 38 | Wire (I2C Bus 1) |
| AS5600 SDA | 8 | Wire1 (I2C Bus 0) |
| AS5600 SCL | 9 | Wire1 (I2C Bus 0) |
| Motor PWM A | 4 | LEDC Ch0 → SimpleFOC Mini IN1 |
| Motor PWM B | 5 | LEDC Ch1 → SimpleFOC Mini IN2 |
| Motor PWM C | 6 | LEDC Ch2 → SimpleFOC Mini IN3 |
| IMU Interrupt | 21 | Motion wake-up |
| AS5600 DIR | 3 | Rotation direction |
| Onboard LED | 48 | BLE status blink |

---

## Repo Structure

```
smart-knob-mouse/
├── Phase 1/                          # IMU communication + cursor control
│   ├── a_i2c_scanner.ino             #   I2C bus scan (find devices)
│   ├── b_IMU_dignostic.ino           #   BMI160 register validation
│   ├── c_phase1_imu_test4.ino        #   Raw accel/gyro serial output
│   ├── d_phase1_imu_cursor.ino       #   IMU data → serial → cursor
│   └── e_imu_cursor_control.py       #   Python: serial → cursor movement
│
├── Phase 2/                          # AS5600 magnetic encoder testing
│   └── phase2_as5600_test.ino        #   Magnet detect + angle sweep + noise check
│
├── Phase 3/                          # Motor driver & open-loop testing
│   ├── phase3_pwm_signal_test.ino    #   PWM signal verification (no 12V needed)
│   ├── phase3_raw_spin_test.ino      #   Open-loop motor spin (12V required)
│   └── phase3_pole_pair_detect3.ino  #   Pole pair detection with AS5600 (→ 7PP)
│
├── Phase 5a/                         # BLE downlink (laptop → ESP32)
│   ├── phase5a_ble_downlink.ino      #   ESP32 BLE GATT server
│   ├── ble_hotkey_downlink3.py       #   Hotkey listener + BLE write
│   └── test_serial_downlink.py       #   Serial-based downlink test
│
├── Phase 5b/                         # BLE uplink (ESP32 → laptop)
│   ├── phase5b_ble_uplink.ino        #   ESP32 sends IMU via BLE notify
│   ├── ble_imu_print.py              #   Print raw IMU data from BLE
│   ├── ble_imu_cursor.py             #   BLE IMU data → cursor movement
│   └── Phase 5 a and b int/          #   Combined downlink + uplink test
│
└── README.md
```

---

## Development Roadmap

The project follows a phased approach, testing each subsystem independently before integration:

- [x] **Phase 1** — IMU communication over I2C + serial cursor control
- [x] **Phase 2** — AS5600 magnetic encoder angle tracking
- [x] **Phase 3** — Motor open-loop PWM + spin test + pole pair detection (confirmed 7PP)
- [ ] **Phase 4** — Closed-loop FOC with AS5600 feedback (SimpleFOC torque mode)
- [x] **Phase 5a** — BLE downlink: laptop sends mode commands → ESP32
- [x] **Phase 5b** — BLE uplink: ESP32 sends IMU data → laptop moves cursor
- [ ] **Phase 5c** — Mode testing with Python GUI (all 5 modes via BLE)
- [ ] **Phase 5d** — Native BLE HID (no Python middleman for mouse/keyboard)
- [ ] **Phase 6** — Mode-by-mode haptic integration (Scroll → Freewheel → Volume → Zoom → Click → Notification)
- [ ] **Phase 7** — Full system: IMU cursor + haptic knob + BLE HID + mode switching

---

## Getting Started

### Prerequisites

**Firmware (Arduino IDE):**

1. Install [Arduino IDE 2.x](https://www.arduino.cc/en/software)
2. Add ESP32 board support URL: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Select board: **ESP32S3 Dev Module**
4. Set **USB CDC On Boot: Enabled** (required for serial output)
5. Install these libraries via Library Manager:
   - `SimpleFOC` (v2.3+) — FOC motor control
   - `NimBLE-Arduino` — lightweight BLE stack

**Python host scripts:**

```bash
pip install bleak pynput pyautogui pyserial
```

### Running Phase 1 (IMU Test)

Hardware needed: ESP32-S3 + BMI160/MPU6050 breakout only (powered via USB).

1. Wire IMU: SDA → GPIO 37, SCL → GPIO 38, 3.3V → **3V3 pin** (not VIN!), GND → GND
2. Upload `Phase 1/a_i2c_scanner.ino` — confirm device detected at `0x69` (BMI160)
3. Upload `Phase 1/c_phase1_imu_test4.ino` — verify accel/gyro values change when board is tilted
4. For cursor control: upload `d_phase1_imu_cursor.ino`, then run `python "Phase 1/e_imu_cursor_control.py"`

### Running Phase 2 (AS5600 Encoder Test)

Hardware needed: ESP32-S3 + AS5600 + diametrically magnetized magnet.

1. Wire AS5600: SDA → GPIO 8, SCL → GPIO 9, VCC → 3V3, GND → GND
2. Mount magnet on shaft, centered 1–2mm above AS5600 IC
3. Upload `Phase 2/phase2_as5600_test.ino` — confirm magnet detected (MD=1) and angle sweeps 0°–360° when rotated

### Running Phase 3 (Motor & Driver Tests)

Hardware needed: Varies per test — see [Phase 3 README](Phase%203/README.md) for details.

1. **Signal test** (no 12V needed): Upload `phase3_pwm_signal_test.ino` — verify PWM at GPIO 4/5/6 with multimeter
2. **Spin test** (12V required): Upload `phase3_raw_spin_test.ino` — motor should spin smoothly
3. **Pole pair test** (12V + AS5600): Upload `phase3_pole_pair_detect3.ino` — should report 7 pole pairs

### Running Phase 5a (BLE Downlink)

Hardware needed: ESP32-S3 only.

1. Upload `Phase 5a/phase5a_ble_downlink.ino`
2. Verify ESP32 appears as "SmartKnob" in nRF Connect
3. Run `python "Phase 5a/ble_hotkey_downlink3.py"` — Ctrl+Alt hotkeys change LED blink rate

### Running Phase 5b (BLE Uplink + Cursor)

Hardware needed: ESP32-S3 + BMI160.

1. Upload `Phase 5b/phase5b_ble_uplink.ino`
2. Run `python "Phase 5b/ble_imu_print.py"` to verify IMU data arrives via BLE
3. Run `python "Phase 5b/ble_imu_cursor.py"` to control cursor by tilting the board

---

## Wiring Notes

A few things that can save debugging time:

- **IMU power:** Connect 3.3V to the breakout's **3V3 pin**, not VIN. VIN expects 5V and feeds an onboard LDO — feeding 3.3V to VIN will brownout the IMU.
- **IMU chip ID:** Your board is labeled MPU6050 but contains a BMI160. Chip ID at register 0x00 = 0xD1 confirms this. Data is LSB-first (opposite of MPU6050).
- **Decoupling caps:** Add 100µF electrolytic on the 12V rail near the driver and 100nF ceramic near each IC's power pins. The boost converter generates switching noise that can corrupt I2C.
- **Common ground:** All components must share a common ground. A floating driver ground causes erratic motor behavior.
- **AS5600 magnet:** Requires a diametrically magnetized magnet centered above the IC on the motor shaft, 1–2mm air gap. Misalignment causes noisy angle readings.
- **ESP32-S3 upload:** Hold the BOOT button during upload if the board doesn't enter programming mode automatically. Enable **USB CDC On Boot** in Arduino IDE board settings.

---

## Key Technical Details

**Dual I2C Buses:** The AS5600 (FOC angle sensor, polled at ~10kHz) and BMI160 (cursor IMU, polled at ~100Hz) are on separate hardware I2C buses to prevent bus contention. The FOC loop cannot afford to wait behind lower-priority IMU transactions.

**Motor Cogging:** The 2204 motor has 84 cogging positions per revolution (4.29° spacing). Virtual detent spacing is kept at ≥15° (~24 detents/rev) so haptic detent torque dominates over mechanical cogging.

**Zero-Offset Mode Switching:** When switching modes, the firmware captures the current shaft angle as the new logical zero — no motor movement, no jerk, instant transition. All torque calculations use `relative_angle = shaft_angle - zero_offset`.

**Power Isolation:** The motor is powered from a USB power bank through a boost converter, completely isolated from the laptop. BLE is the only connection, so motor noise cannot reach the laptop's USB port.

**Core Pinning:** The FOC loop runs on Core 0 (deterministic, no BLE interrupts). BLE stack and IMU processing run on Core 1. A FreeRTOS queue passes events between cores without blocking.

---

## Torque Equations

Each mode applies a different torque function based on shaft position θ (relative to mode-switch zero):

| Mode | Torque Equation | Key Parameter |
|------|----------------|---------------|
| Scroll | `T = -K_detent × sin(2π × θ / spacing)` | K_detent ≈ 0.5–1.0V, spacing ≈ 15° |
| Volume | `T = 0` in range; `T = -K_wall × (θ - limit)` at ends | K_wall ≈ 3–5V |
| Zoom | `T = -K_spring × θ` | K_spring ≈ 1–2V |
| Freewheel | `T = 0` (motor disabled) | — |
| Click | `T = -K_click × sin(2π × θ / detent_width)` | K_click ≈ 2–3V, width ≈ 10° |

---

## Acknowledgments

- [Scott Bezek's SmartKnob](https://github.com/scottbez1/smartknob) — original concept and inspiration
- [SimpleFOC](https://simplefoc.com/) — FOC motor control library for Arduino
- [Bleak](https://github.com/hbldh/bleak) — cross-platform Python BLE library

## License

MIT
