"""
Phase 1 - IMU Cursor Control (Python Side)

Reads IMU data from ESP32 over Serial.
Two modes:
  --print    Just print raw IMU values (test connection)
  --cursor   Move the system cursor by tilting the board

Usage:
  pip install pyserial pyautogui
  python imu_cursor_control.py --print     (test first)
  python imu_cursor_control.py --cursor    (move cursor)

Author: Yogeshwaran
"""

import serial
import serial.tools.list_ports
import sys
import time

# ── Settings ──
BAUD = 115200
DEAD_ZONE = 3000      # Ignore small tilts (raw accel units)
SENSITIVITY = 0.001   # How fast cursor moves (tune this)
SMOOTHING = 0.2       # 0 = very smooth, 1 = instant (tune this)


def find_port():
    """Auto-detect ESP32 serial port."""
    ports = serial.tools.list_ports.comports()
    for p in ports:
        if any(k in p.description.lower() for k in
               ["cp210", "ch340", "ftdi", "usb serial", "esp32", "usb jtag"]):
            return p.device
    # If no auto-detect, show available ports
    print("Available ports:")
    for p in ports:
        print(f"  {p.device}: {p.description}")
    return input("Enter COM port: ").strip()


def parse_line(line):
    """Parse IMU,accX,accY,accZ,gyroX,gyroY,gyroZ"""
    try:
        parts = line.strip().split(",")
        if parts[0] != "IMU" or len(parts) != 7:
            return None
        return {
            "accX":  int(parts[1]),
            "accY":  int(parts[2]),
            "accZ":  int(parts[3]),
            "gyroX": int(parts[4]),
            "gyroY": int(parts[5]),
            "gyroZ": int(parts[6]),
        }
    except (ValueError, IndexError):
        return None


def run_print_mode(ser):
    """Just print IMU data to terminal."""
    print("\n--- Raw IMU Data (Ctrl+C to stop) ---")
    print(f"{'AccX':>8} {'AccY':>8} {'AccZ':>8} {'GyroX':>8} {'GyroY':>8} {'GyroZ':>8}")
    print("-" * 54)

    while True:
        line = ser.readline().decode("utf-8", errors="replace").strip()
        data = parse_line(line)
        if data:
            print(f"{data['accX']:>8} {data['accY']:>8} {data['accZ']:>8} "
                  f"{data['gyroX']:>8} {data['gyroY']:>8} {data['gyroZ']:>8}")


def run_cursor_mode(ser):
    """Move system cursor based on board tilt."""
    try:
        import pyautogui
    except ImportError:
        print("Install pyautogui first: pip install pyautogui")
        return

    pyautogui.FAILSAFE = True  # Move mouse to corner to stop
    pyautogui.PAUSE = 0        # No delay between moves

    print("\n--- Cursor Control Mode ---")
    print("Tilt board to move cursor")
    print("Move mouse to top-left corner to emergency stop")
    print("Ctrl+C to quit\n")

    # Calibration: read a few samples to get the resting position
    print("Hold board still for calibration...", end="", flush=True)
    cal_x, cal_y = 0, 0
    cal_count = 20

    for i in range(cal_count):
        line = ser.readline().decode("utf-8", errors="replace").strip()
        data = parse_line(line)
        if data:
            cal_x += data["accX"]
            cal_y += data["accY"]

    cal_x //= cal_count
    cal_y //= cal_count
    print(f" done! (rest: X={cal_x}, Y={cal_y})")
    print("Move the board now!\n")

    smooth_dx = 0.0
    smooth_dy = 0.0

    while True:
        line = ser.readline().decode("utf-8", errors="replace").strip()
        data = parse_line(line)
        if not data:
            continue

        # Subtract calibration offset
        raw_x = data["accX"] - cal_x
        raw_y = data["accY"] - cal_y

        # Apply dead zone
        if abs(raw_x) < DEAD_ZONE:
            raw_x = 0
        if abs(raw_y) < DEAD_ZONE:
            raw_y = 0

        # Convert to cursor movement (pixels)
        # Y axis flipped to match natural tilt direction
        dx = raw_x * SENSITIVITY
        dy = -raw_y * SENSITIVITY

        # Smooth the movement
        smooth_dx = smooth_dx * (1 - SMOOTHING) + dx * SMOOTHING
        smooth_dy = smooth_dy * (1 - SMOOTHING) + dy * SMOOTHING

        # Clamp movement so cursor can't fly across screen
        move_x = max(-20, min(20, int(smooth_dx)))
        move_y = max(-20, min(20, int(smooth_dy)))

        if move_x != 0 or move_y != 0:
            try:
                pyautogui.moveRel(move_x, move_y, _pause=False)
            except pyautogui.FailSafeException:
                # Cursor hit a corner — reset smoothing and continue
                smooth_dx = 0.0
                smooth_dy = 0.0


def main():
    # Check mode
    mode = "--print"
    if len(sys.argv) > 1:
        mode = sys.argv[1]

    if mode not in ("--print", "--cursor"):
        print("Usage:")
        print("  python imu_cursor_control.py --print   (show raw data)")
        print("  python imu_cursor_control.py --cursor  (move cursor)")
        return

    # Connect to ESP32
    port = find_port()
    print(f"Connecting to {port}...", end="", flush=True)

    try:
        ser = serial.Serial(port, BAUD, timeout=1)
    except serial.SerialException as e:
        print(f" failed: {e}")
        print("Close Arduino Serial Monitor first!")
        return

    time.sleep(2)  # Wait for ESP32 reset

    # Wait for READY signal
    print(" connected!")
    print("Waiting for ESP32...", end="", flush=True)
    for _ in range(30):
        line = ser.readline().decode("utf-8", errors="replace").strip()
        if line == "READY":
            print(" ready!")
            break
    else:
        print(" no READY signal, continuing anyway...")

    # Run selected mode
    try:
        if mode == "--print":
            run_print_mode(ser)
        else:
            run_cursor_mode(ser)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
