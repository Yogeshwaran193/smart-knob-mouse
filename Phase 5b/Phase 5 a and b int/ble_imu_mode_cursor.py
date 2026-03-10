"""
Phase 5b - Step 2: BLE IMU Cursor Control (Wireless)
ESP32 sends IMU data over BLE, this script moves the cursor.

Also supports mode switching via hotkeys (Phase 5a).

Usage:
  pip install bleak pyautogui keyboard
  Run PowerShell as Administrator, then:
  python ble_imu_cursor.py

Author: Yogeshwaran
"""

import asyncio
import struct
import keyboard
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
MODE_CHAR_UUID     = "12345678-1234-5678-1234-56789abcdef1"
IMU_DATA_CHAR_UUID = "12345678-1234-5678-1234-56789abcdef2"

# ── Cursor settings (tune these) ──
DEAD_ZONE   = 3000    # Ignore small tilts
SENSITIVITY = 0.001   # Cursor speed
SMOOTHING   = 0.2     # 0=smooth, 1=instant
MAX_MOVE    = 20      # Max pixels per frame

# ── Mode IDs ──
MODES = {
    "ctrl+alt+s": (0x01, "SCROLL",    "Blue"),
    "ctrl+alt+v": (0x02, "VOLUME",    "Green"),
    "ctrl+alt+z": (0x03, "ZOOM",      "Red"),
    "ctrl+alt+f": (0x04, "FREEWHEEL", "White"),
    "ctrl+alt+c": (0x05, "CLICK",     "Purple"),
}

# ── State ──
smooth_dx = 0.0
smooth_dy = 0.0
cal_x = 0
cal_y = 0
calibrated = False
cal_samples = []
pending_mode = None
packet_count = 0


def on_imu_data(sender, data):
    """BLE notification callback — moves cursor based on tilt."""
    global smooth_dx, smooth_dy, cal_x, cal_y, calibrated, cal_samples, packet_count
    packet_count += 1

    if len(data) < 6:
        return

    accX, accY, accZ = struct.unpack("<hhh", data[:6])

    # Calibration: collect first 30 samples to find resting position
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

    # Scale to pixels
    dx = raw_x * SENSITIVITY
    dy = -raw_y * SENSITIVITY  # Y flipped

    # Smooth
    smooth_dx = smooth_dx * (1 - SMOOTHING) + dx * SMOOTHING
    smooth_dy = smooth_dy * (1 - SMOOTHING) + dy * SMOOTHING

    # Clamp
    move_x = max(-MAX_MOVE, min(MAX_MOVE, int(smooth_dx)))
    move_y = max(-MAX_MOVE, min(MAX_MOVE, int(smooth_dy)))

    if move_x != 0 or move_y != 0:
        try:
            pyautogui.moveRel(move_x, move_y, _pause=False)
        except pyautogui.FailSafeException:
            smooth_dx = 0.0
            smooth_dy = 0.0


def on_hotkey(hotkey_str):
    """Hotkey pressed — queue mode change."""
    global pending_mode
    mode_id, name, color = MODES[hotkey_str]
    pending_mode = mode_id
    print(f"  Mode -> {name} ({color})")


async def main():
    global pending_mode

    print("=== Phase 5b: BLE Wireless Cursor ===\n")

    print("  Hotkeys:")
    for key, (mid, name, color) in MODES.items():
        print(f"    {key.upper():16s} -> {name} ({color})")
    print(f"    {'CTRL+ALT+Q':16s} -> Quit\n")

    # Register hotkeys
    for hk in MODES:
        keyboard.add_hotkey(hk, on_hotkey, args=[hk])

    quit_pressed = False
    def on_quit():
        nonlocal quit_pressed
        quit_pressed = True
    keyboard.add_hotkey("ctrl+alt+q", on_quit)

    # Find ESP32
    print("  Scanning...", end="", flush=True)
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10)

    if not device:
        print(" not found.")
        keyboard.unhook_all()
        return

    print(f" found! ({device.address})")
    print("  Connecting...", end="", flush=True)

    async with BleakClient(device.address) as client:
        print(" connected!\n")
        print("  Hold board still for calibration (1 sec)...")

        # Subscribe to IMU data
        await client.start_notify(IMU_DATA_CHAR_UUID, on_imu_data)

        # Main loop
        while not quit_pressed:
            # Send mode changes
            if pending_mode is not None:
                mode_to_send = pending_mode
                pending_mode = None
                try:
                    await client.write_gatt_char(
                        MODE_CHAR_UUID,
                        bytes([mode_to_send]),
                        response=False
                    )
                except Exception as e:
                    print(f"  BLE write error: {e}")

            await asyncio.sleep(0.05)

        await client.stop_notify(IMU_DATA_CHAR_UUID)

    keyboard.unhook_all()
    print(f"\n  Done. Packets received: {packet_count}")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        keyboard.unhook_all()
        print(f"\n  Stopped. Packets: {packet_count}")
