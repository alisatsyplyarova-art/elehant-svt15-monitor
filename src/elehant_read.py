import sys
from pathlib import Path
import asyncio
from datetime import datetime

from bleak import BleakScanner

# Подключаем оригинальный parser.py из проекта raxers/elehant_water
PROJECT_ROOT = Path(__file__).resolve().parent.parent
PARSER_DIR = PROJECT_ROOT / "third_party" / "elehant_water" / "custom_components" / "elehant_water"
sys.path.insert(0, str(PARSER_DIR))

from parser import parse_manufacturer_data


def callback(device, advertisement_data):
    if 0xFFFF not in advertisement_data.manufacturer_data:
        return

    try:
        reading = parse_manufacturer_data(
            device.address,
            advertisement_data.manufacturer_data
        )
    except Exception:
        return

    if reading is None:
        return

    now = datetime.now().strftime("%H:%M:%S")

    print()
    print("=" * 70)
    print(f"[{now}] ELEHANT")
    print(f"Bluetooth : {device.address}")
    print(f"RSSI      : {advertisement_data.rssi}")
    print(f"Название  : {reading.title}")
    print(f"Серийный  : {reading.serial}")
    print(f"Версия    : {reading.packet_version}")
    print(f"Прошивка  : {reading.firmware}")
    print("Показания :")

    for key, value in reading.values.items():
        print(f"  {key:20} = {value}")

    print("=" * 70)


async def main():
    print("=" * 70)
    print(" ELEHANT SVT-15 — ПРЯМОЙ BLE МОНИТОР")
    print(" Используется parser.py из raxers/elehant_water")
    print(" Подключение к счётчику НЕ выполняется")
    print(" Ожидание BLE-рекламных пакетов...")
    print("=" * 70)

    scanner = BleakScanner(detection_callback=callback)

    await scanner.start()

    try:
        while True:
            await asyncio.sleep(1)
    except (KeyboardInterrupt, asyncio.CancelledError):
        pass
    finally:
        await scanner.stop()

    print()
    print("Монитор остановлен.")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass


