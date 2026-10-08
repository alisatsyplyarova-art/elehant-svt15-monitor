import asyncio
from bleak import BleakScanner


async def main():
    print("======================================")
    print(" BLE SCAN — Elehant SVT-15")
    print(" Сканирование 60 секунд...")
    print("======================================")
    print()

    devices = await BleakScanner.discover(
        timeout=60,
        return_adv=True
    )

    print()
    print(f"Найдено устройств: {len(devices)}")
    print()

    for address, (device, adv) in devices.items():
        print("=" * 70)
        print(f"ADDRESS : {address}")
        print(f"NAME    : {adv.local_name or device.name}")
        print(f"RSSI    : {adv.rssi}")

        print("MFG DATA:")
        if adv.manufacturer_data:
            for company_id, data in adv.manufacturer_data.items():
                print(
                    f"  Company ID: 0x{company_id:04X} "
                    f"DATA: {data.hex(' ')}"
                )
        else:
            print("  <нет>")

        print("SERVICE DATA:")
        if adv.service_data:
            for uuid, data in adv.service_data.items():
                print(
                    f"  UUID: {uuid} "
                    f"DATA: {data.hex(' ')}"
                )
        else:
            print("  <нет>")

    print()
    print("=" * 70)
    print("Сканирование завершено.")


if __name__ == "__main__":
    asyncio.run(main())