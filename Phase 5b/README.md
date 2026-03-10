# Phase 5b: BLE Uplink (ESP32 → Laptop)

**Goal:** ESP32 reads IMU data and sends it to the laptop over BLE notifications. Python script receives the data and moves the system cursor.

**Hardware needed:** ESP32-S3 + MPU6050

**Status:** ✅ Complete

## BLE Configuration

| Parameter | Value |
|-----------|-------|
| Device Name | `SmartKnob` |
| Service UUID | `12345678-1234-1234-1234-123456789abc` |
| IMU Characteristic UUID | `12345678-1234-1234-1234-123456789abe` |
| Data Format | 6 bytes: accelX(2) + accelY(2) + gyroZ(2), big-endian int16 |
| Notify Rate | ~100Hz |

## Files

| File | Pairs With | Purpose |
|------|-----------|---------|
| `phase5b_ble_uplink.ino` | Any Python script below | ESP32 reads IMU, sends via BLE notify |
| `ble_imu_print.py` | `phase5b_ble_uplink.ino` | Print raw IMU values to terminal (simple test) |
| `ble_imu_cursor.py` | `phase5b_ble_uplink.ino` | Move system cursor based on board tilt |

### `Phase 5 a and b int/` subfolder

Combined integration test: downlink mode switching (Phase 5a) + uplink IMU cursor (Phase 5b) running simultaneously.

## Quick Start Pairings

| Test | Firmware | Python Script |
|------|----------|---------------|
| Simple data verify | `phase5b_ble_uplink.ino` | `ble_imu_print.py` |
| Cursor control | `phase5b_ble_uplink.ino` | `ble_imu_cursor.py` |
| Full 5a+5b combined | See `Phase 5 a and b int/` | See `Phase 5 a and b int/` |

## Pass Criteria

- IMU notifications received at ~100Hz on the laptop
- Tilting the ESP32 board moves the cursor in the corresponding direction
- Connection is stable for >1 minute without drops
- End-to-end latency < 50ms (tilt → cursor movement)

## Python Dependencies

```bash
pip install bleak pyautogui
```
