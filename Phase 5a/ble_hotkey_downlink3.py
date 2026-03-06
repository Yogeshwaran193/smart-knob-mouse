"""
Phase 5a — BLE Hotkey Downlink (Windows-friendly)
Uses 'keyboard' library instead of pynput — works better on Windows.

MUST run PowerShell as Administrator:
  1. Right-click PowerShell, Run as Administrator
  2. cd to your Downloads folder
  3. python ble_hotkey_downlink.py

Requirements:
  pip install bleak keyboard
"""

import asyncio
import keyboard
from bleak import BleakClient, BleakScanner

# BLE config
DEVICE_NAME    = "SmartKnobMouse"
MODE_CHAR_UUID = "12345678-1234-5678-1234-56789abcdef1"

# Modes
MODES = {
    "ctrl+alt+s": (0x01, "SCROLL",    "Blue,  slow blink"),
    "ctrl+alt+v": (0x02, "VOLUME",    "Green, double blink"),
    "ctrl+alt+z": (0x03, "ZOOM",      "Red,   fast blink"),
    "ctrl+alt+f": (0x04, "FREEWHEEL", "White, solid on"),
    "ctrl+alt+c": (0x05, "CLICK",     "Purple, single flash"),
}

# Shared state
pending_mode = None


def on_hotkey(hotkey_str):
    """Called when a hotkey is pressed."""
    global pending_mode
    mode_id, name, color = MODES[hotkey_str]
    pending_mode = mode_id
    print(f"  ► {name} → {color}")


async def main():
    global pending_mode

    print("╔══════════════════════════════════════════════╗")
    print("║  Smart Knob Mouse — BLE Hotkey Downlink     ║")
    print("║  Phase 5a Wireless                          ║")
    print("╚══════════════════════════════════════════════╝")
    print()
    print("  Ctrl+Alt+S  →  SCROLL    (blue)")
    print("  Ctrl+Alt+V  →  VOLUME    (green)")
    print("  Ctrl+Alt+Z  →  ZOOM      (red)")
    print("  Ctrl+Alt+F  →  FREEWHEEL (white)")
    print("  Ctrl+Alt+C  →  CLICK     (purple)")
    print("  Ctrl+Alt+Q  →  Quit")
    print()

    # Register hotkeys
    for hotkey_str in MODES:
        keyboard.add_hotkey(hotkey_str, on_hotkey, args=[hotkey_str])

    quit_pressed = False

    def on_quit():
        nonlocal quit_pressed
        quit_pressed = True
        print("\n  Ctrl+Alt+Q → shutting down...")

    keyboard.add_hotkey("ctrl+alt+q", on_quit)

    # Scan for ESP32
    print("  Scanning for SmartKnobMouse...", end="", flush=True)
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10)

    if not device:
        print(" not found. Is ESP32 powered on?")
        keyboard.unhook_all()
        return

    print(f" found! ({device.address})")
    print("  Connecting...", end="", flush=True)

    async with BleakClient(device.address) as client:
        print(" connected!")
        print()
        print("  Press hotkeys now. Listening...\n")

        while not quit_pressed:
            # Check if a hotkey set a new mode
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
                    print(f"  BLE error: {e}")
                    break

            # Small sleep so we don't burn CPU
            await asyncio.sleep(0.05)

    keyboard.unhook_all()
    print("  Done.")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        keyboard.unhook_all()
        print("\n  Done.")
