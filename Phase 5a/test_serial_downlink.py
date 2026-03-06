"""
============================================================================
 Phase 5a — Serial Mode Sender (Wired Test)
============================================================================
 Smart Knob Mouse | Laptop Side

 Quick wired test: sends mode commands to ESP32-S3 over USB Serial.
 Use this BEFORE setting up BLE to verify the ESP firmware is receiving
 and applying modes correctly.

 Usage:
   1. Flash phase5a_ble_downlink.ino to ESP32-S3
   2. pip install pyserial keyboard
   3. python test_serial_downlink.py
   4. Press hotkeys (Ctrl+Alt+S/V/Z/F/C) or type mode names

 Requirements:
   pip install pyserial keyboard

 Note: The 'keyboard' library requires root/admin on Linux.
       On Linux:  sudo python test_serial_downlink.py
       On Windows: run as Administrator
       On macOS:   use 'pynput' version instead (see BLE script)

 Author: Yogeshwaran
============================================================================
"""

import serial
import serial.tools.list_ports
import keyboard
import time
import sys

# ============================================================================
#  Configuration
# ============================================================================

# Mode definitions — must match ESP32 firmware
MODES = {
    "scroll":    {"id": 0x01, "hotkey": "ctrl+alt+s", "label": "SCROLL    (24 det, medium)"},
    "volume":    {"id": 0x02, "hotkey": "ctrl+alt+v", "label": "VOLUME    (16 det, strong + endstops)"},
    "zoom":      {"id": 0x03, "hotkey": "ctrl+alt+z", "label": "ZOOM      (12 det, light)"},
    "freewheel": {"id": 0x04, "hotkey": "ctrl+alt+f", "label": "FREEWHEEL (smooth, no detents)"},
    "click":     {"id": 0x05, "hotkey": "ctrl+alt+c", "label": "CLICK     (center detent, spring)"},
}

BAUD_RATE = 9600 #115200
current_mode = "scroll"


# ============================================================================
#  Serial Port Auto-Detection
# ============================================================================
def find_esp32_port():
    """Scan COM ports for ESP32-S3. Returns port name or None."""
    ports = serial.tools.list_ports.comports()
    esp_ports = []

    print("\n── Scanning serial ports ──")
    for port in ports:
        desc = f"  {port.device}: {port.description}"
        # ESP32-S3 typically shows up with these VID/PID or descriptions
        if any(keyword in port.description.lower() for keyword in
               ["cp210", "ch340", "ftdi", "usb serial", "esp32", "usb jtag"]):
            desc += "  ← likely ESP32"
            esp_ports.append(port.device)
        print(desc)

    if len(esp_ports) == 1:
        print(f"\n  Auto-selected: {esp_ports[0]}")
        return esp_ports[0]
    elif len(esp_ports) > 1:
        print(f"\n  Multiple candidates found: {esp_ports}")
        return esp_ports[0]  # Take first match
    else:
        print("\n  No ESP32 detected automatically.")
        return None


def connect_serial():
    """Find and open serial connection to ESP32."""
    port = find_esp32_port()

    if port is None:
        port = input("  Enter COM port manually (e.g., COM3 or /dev/ttyUSB0): ").strip()

    try:
        ser = serial.Serial(port, BAUD_RATE, timeout=0.1)
        time.sleep(2)  # Wait for ESP32 to reset after serial open
        # Flush any boot messages
        while ser.in_waiting:
            print(f"  [ESP32] {ser.readline().decode('utf-8', errors='replace').strip()}")
        print(f"\n  Connected to {port} @ {BAUD_RATE} baud\n")
        return ser
    except serial.SerialException as e:
        print(f"\n  ERROR: Could not open {port}: {e}")
        sys.exit(1)


# ============================================================================
#  Send Mode Command
# ============================================================================
def send_mode(ser, mode_name):
    """Send mode command string over serial."""
    global current_mode

    if mode_name not in MODES:
        print(f"  Unknown mode: {mode_name}")
        return

    if mode_name == current_mode:
        print(f"  Already in {mode_name.upper()} mode")
        return

    mode = MODES[mode_name]
    # Send the text command (ESP firmware parses both text and single-char)
    cmd = mode_name.upper() + "\n"
    ser.write(cmd.encode())
    current_mode = mode_name

    print(f"  ► Sent: {mode['label']}")

    # Read ESP32 response
    time.sleep(0.1)
    while ser.in_waiting:
        line = ser.readline().decode("utf-8", errors="replace").strip()
        if line:
            print(f"  [ESP32] {line}")


# ============================================================================
#  Hotkey Registration
# ============================================================================
def register_hotkeys(ser):
    """Register Ctrl+Alt+<key> hotkeys for mode switching."""
    for mode_name, mode_info in MODES.items():
        # Use default argument capture to avoid closure issue
        keyboard.add_hotkey(
            mode_info["hotkey"],
            lambda m=mode_name: send_mode(ser, m),
            suppress=True
        )


# ============================================================================
#  Main
# ============================================================================
def main():
    print("╔══════════════════════════════════════════════╗")
    print("║  Smart Knob Mouse — Serial Mode Test        ║")
    print("║  Phase 5a Wired Downlink                    ║")
    print("╚══════════════════════════════════════════════╝")

    ser = connect_serial()

    # Register global hotkeys
    register_hotkeys(ser)

    # Print hotkey map
    print("── Hotkey Map ──────────────────────────────────")
    for mode_name, mode_info in MODES.items():
        marker = " ◄" if mode_name == current_mode else ""
        print(f"  {mode_info['hotkey'].upper():16s} → {mode_info['label']}{marker}")
    print()
    print("  Press Ctrl+Alt+Q to quit")
    print("  Type mode name (scroll/volume/zoom/freewheel/click) + Enter")
    print("────────────────────────────────────────────────")
    print()

    # Also allow typing mode names in terminal
    try:
        while True:
            # Check for typed input (non-blocking with timeout)
            if sys.stdin.readable():
                # keyboard.wait handles hotkeys in background
                # We also accept typed commands
                try:
                    line = input().strip().lower()
                    if line in MODES:
                        send_mode(ser, line)
                    elif line in ("q", "quit", "exit"):
                        break
                    elif line == "?":
                        print(f"\n  Current mode: {current_mode.upper()}")
                        print(f"  BLE connected: check ESP32 serial monitor\n")
                    elif line:
                        print(f"  Unknown: '{line}' — try: scroll, volume, zoom, freewheel, click")
                except EOFError:
                    break

            # Read any async ESP32 messages
            while ser.in_waiting:
                line = ser.readline().decode("utf-8", errors="replace").strip()
                if line:
                    print(f"  [ESP32] {line}")

    except KeyboardInterrupt:
        pass
    finally:
        print("\n  Closing serial connection...")
        keyboard.unhook_all()
        ser.close()
        print("  Done.")


if __name__ == "__main__":
    main()
