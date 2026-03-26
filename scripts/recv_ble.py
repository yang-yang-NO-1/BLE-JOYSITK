"""
Debian BLE receiver for the ESP32 joystick example.

Requirements:
    sudo apt install -y bluetooth bluez python3-pip
    pip3 install bleak
"""

import asyncio
from bleak import BleakClient, BleakScanner

DEVICE_NAME = "ESP32_Gamepad"
CHAR_UUID = "abcd1234-5678-1234-5678-1234567890ab"


def handle_notify(_sender, data: bytearray) -> None:
    if len(data) != 5:
        print("invalid packet:", data.hex(" "))
        return

    joy_x = data[0] | (data[1] << 8)
    joy_y = data[2] | (data[3] << 8)
    buttons = data[4]

    sw = 1 if (buttons & 0x01) else 0
    btn1 = 1 if (buttons & 0x02) else 0
    btn2 = 1 if (buttons & 0x04) else 0

    print(f"X={joy_x:4d} Y={joy_y:4d} SW={sw} BTN1={btn1} BTN2={btn2}")


async def main() -> None:
    print("scanning...")
    devices = await BleakScanner.discover(timeout=5.0)

    target = next((d for d in devices if (d.name or "") == DEVICE_NAME), None)
    if target is None:
        print(f"device '{DEVICE_NAME}' not found")
        return

    print("connecting to:", target.address)
    async with BleakClient(target.address) as client:
        print("connected:", client.is_connected)
        await client.start_notify(CHAR_UUID, handle_notify)
        print("receiving... Ctrl+C to stop")
        while True:
            await asyncio.sleep(1)


if __name__ == "__main__":
    asyncio.run(main())
