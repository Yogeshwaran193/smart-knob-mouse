"""
Phase 5b — Step 1: Print IMU data received over BLE
Verifies the uplink path: ESP32 → BLE notify → laptop

Usage:
  pip install bleak
  python ble_imu_print.py

Author: Yogeshwaran
"""

import asyncio
import struct
from bleak import BleakClient, BleakScanner

DEVICE_NAME      = "SmartKnobMouse"
IMU_DATA_CHAR_UUID = "12345678-1234-5678-1234-56789abcdef2"

packet_count = 0


def on_imu_data(sender, data):
    """Called every time ESP32 sends a BLE notification."""
    global packet_count
    packet_count += 1

    if len(data) >= 6:
        # Unpack 3 little-endian int16 values
        accX, accY, accZ = struct.unpack("<hhh", data[:6])
        # Print every 5th packet to keep terminal readable
        if packet_count % 5 == 0:
            print(f"  AccX={accX:>7}  AccY={accY:>7}  AccZ={accZ:>7}  "
                  f"(packets: {packet_count})")


async def main():
    print("=== Phase 5b: BLE IMU Print Test ===\n")

    print("  Scanning...", end="", flush=True)
    device = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10)

    if not device:
        print(" not found. Is ESP32 powered?")
        return

    print(f" found! ({device.address})")
    print("  Connecting...", end="", flush=True)

    async with BleakClient(device.address) as client:
        print(" connected!\n")

        # Subscribe to IMU notifications
        await client.start_notify(IMU_DATA_CHAR_UUID, on_imu_data)
        print("  Listening for IMU data (Ctrl+C to stop)...")
        print("  Tilt the board to see values change!\n")
        print(f"  {'AccX':>7}  {'AccY':>7}  {'AccZ':>7}")
        print(f"  {'----':>7}  {'----':>7}  {'----':>7}")

        # Keep running until Ctrl+C
        try:
            while True:
                await asyncio.sleep(1)
        except asyncio.CancelledError:
            pass

        await client.stop_notify(IMU_DATA_CHAR_UUID)

    print(f"\n  Total packets received: {packet_count}")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print(f"\n  Stopped. Total packets: {packet_count}")
