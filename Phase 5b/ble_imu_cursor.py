"""
Phase 5b - BLE IMU Cursor (cursor only, no mode switching)
Receives IMU data over BLE and moves the system cursor.

Usage:
  pip install bleak pyautogui
  python ble_imu_cursor.py

Author: Yogeshwaran
"""

import asyncio
import struct
from bleak import BleakClient, BleakScanner

try:
    import pyautogui
    pyautogui.FAILSAFE = True
    pyautogui.PAUSE = 0
except ImportError:
    print("Install pyautogui: pip install pyautogui")
    exit(1)

# ── BLE config ──
DEVICE_NAME        = "SmartKnobMouse"
IMU_DATA_CHAR_UUID = "12345678-1234-5678-1234-56789abcdef2"

# ── Cursor settings (tune these) ──
DEAD_ZONE   = 3000
SENSITIVITY = 0.001
SMOOTHING   = 0.2
MAX_MOVE    = 20

# ── State ──
smooth_dx = 0.0
smooth_dy = 0.0
cal_x = 0
cal_y = 0
calibrated = False
cal_samples = []
packet_count = 0


def on_imu_data(sender, data):
    """BLE callback — moves cursor based on tilt."""
    global smooth_dx, smooth_dy, cal_x, cal_y, calibrated, cal_samples, packet_count
    packet_count += 1

    if len(data) < 6:
        return

    accX, accY, accZ = struct.unpack("<hhh", data[:6])

    # Calibration: first 30 samples
    if not calibrated:
        cal_samples.append((accX, accY))
        if len(cal_samples) >= 30:
            cal_x = sum(s[0] for s in cal_samples) // len(cal_samples)
            cal_y = sum(s[1] for s in cal_samples) // len(cal_samples)
            calibrated = True
            print(f"  Calibrated! (rest: X={cal_x}, Y={cal_y})")
            print("  Tilt the board to move cursor!\n")
        return

    # Subtract resting offset
    raw_x = accX - cal_x
    raw_y = accY - cal_y

    # Dead zone
    if abs(raw_x) < DEAD_ZONE:
        raw_x = 0
    if abs(raw_y) < DEAD_ZONE:
        raw_y = 0

    # Scale and flip Y
    dx = raw_x * SENSITIVITY
    dy = -raw_y * SENSITIVITY

    # Smooth
    smooth_dx = smooth_dx * (1 - SMOOTHING) + dx * SMOOTHING
    smooth_dy = smooth_dy * (1 - SMOOTHING) + dy * SMOOTHING

    # Clamp and move
    move_x = max(-MAX_MOVE, min(MAX_MOVE, int(smooth_dx)))
    move_y = max(-MAX_MOVE, min(MAX_MOVE, int(smooth_dy)))

    if move_x != 0 or move_y != 0:
        try:
            pyautogui.moveRel(move_x, move_y, _pause=False)
        except pyautogui.FailSafeException:
            smooth_dx = 0.0
            smooth_dy = 0.0


async def main():
    print("=== Phase 5b: BLE Wireless Cursor ===\n")

    print("  Scanning...", end="", flush=True)
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10)

    if not device:
        print(" not found. Is ESP32 powered?")
        return

    print(f" found! ({device.address})")
    print("  Connecting...", end="", flush=True)

    async with BleakClient(device.address) as client:
        print(" connected!\n")
        print("  Hold board still for calibration (1 sec)...")

        await client.start_notify(IMU_DATA_CHAR_UUID, on_imu_data)

        try:
            while True:
                await asyncio.sleep(1)
        except asyncio.CancelledError:
            pass

        await client.stop_notify(IMU_DATA_CHAR_UUID)

    print(f"\n  Packets received: {packet_count}")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print(f"\n  Stopped. Packets: {packet_count}")
