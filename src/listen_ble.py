import asyncio
from datetime import datetime
from bleak import BleakScanner


def callback(device, advertisement_data):
    timestamp = datetime.now().strftime("%H:%M:%S")

    if advertisement_data.manufacturer_data:
        print()
        print(f"[{timestamp}] {device.address}")
        print(f"NAME: {advertisement_data.local_name or device.name}")
        print(f"RSSI: {advertisement_data.rssi}")

        for company_id, data in advertisement_data.manufacturer_data.items():
            print(f"MFG  : 0x{company_id:04X}  {data.hex(' ')}")

            # Elehant из GitHub использует manufacturer ID 0xFFFF
            if company_id == 0xFFFF:
                print("*** ВОЗМОЖНО ELEHANT ***")


async def main():
    print("=" * 70)
    print("ELEHANT SVT-15 BLE MONITOR")
    print("Слушаем Bluetooth 5 минут")
    print("Не подключаемся к устройствам")
    print("=" * 70)

    scanner = BleakScanner(detection_callback=callback)

    await scanner.start()

    try:
        await asyncio.sleep(300)
    finally:
        await scanner.stop()

    print()
    print("=" * 70)
    print("Сканирование завершено")
    print("=" * 70)


if __name__ == "__main__":
    asyncio.run(main())