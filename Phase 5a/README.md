# Phase 5a: BLE Downlink (Laptop → ESP32)

**Goal:** Verify BLE communication from laptop to ESP32. Python script sends mode commands via hotkeys, ESP32 responds by changing LED blink rate.

**Hardware needed:** ESP32-S3 only (no sensors or motor required)

**Status:** ✅ Complete

## BLE Configuration

| Parameter | Value |
|-----------|-------|
| Device Name | `SmartKnob` |
| Service UUID | `12345678-1234-1234-1234-123456789abc` |
| Mode Characteristic UUID | `12345678-1234-1234-1234-123456789abd` |
| Characteristic Properties | Read + Write |

## Files

| File | Purpose | How to Run |
|------|---------|------------|
| `phase5a_ble_downlink.ino` | ESP32 BLE GATT server — receives mode byte, blinks LED | Upload to ESP32 |
| `ble_hotkey_downlink3.py` | Listens for Ctrl+Alt hotkeys, sends mode byte via BLE | `python ble_hotkey_downlink3.py` |
| `test_serial_downlink.py` | Serial-based downlink test (for debugging without BLE) | `python test_serial_downlink.py` |

## Hotkey → Mode Mapping

| Hotkey | Mode | Byte Value | LED Blink Rate |
|--------|------|------------|----------------|
| Ctrl+Alt+S | Scroll | 0x01 | Slow (1000ms) |
| Ctrl+Alt+V | Volume | 0x02 | Medium-slow |
| Ctrl+Alt+Z | Zoom | 0x03 | Medium |
| Ctrl+Alt+F | Freewheel | 0x04 | Medium-fast |
| Ctrl+Alt+C | Click | 0x05 | Fast (100ms) |

## Pass Criteria

- ESP32 appears as "SmartKnob" in nRF Connect or BLE scanner
- Python Bleak script connects within ~2 seconds
- Writing mode values 1–5 changes LED blink rate within 1 second
- Hotkey presses on laptop trigger immediate LED rate change on ESP32

## Python Dependencies

```bash
pip install bleak pynput
```
