import sys
from pathlib import Path
import asyncio
import csv
import os
from datetime import datetime

from bleak import BleakScanner

PROJECT_ROOT = Path(sys.executable).resolve().parent if getattr(sys, "frozen", False) else Path(__file__).resolve().parent.parent
PARSER_DIR = PROJECT_ROOT / "third_party" / "elehant_water" / "custom_components" / "elehant_water"

sys.path.insert(0, str(PARSER_DIR))

from parser import parse_manufacturer_data


CSV_FILE = str(PROJECT_ROOT / "elehant_history.csv")

last_values = {}


def ensure_csv():
    if not os.path.exists(CSV_FILE):
        with open(
            CSV_FILE,
            "w",
            newline="",
            encoding="utf-8-sig"
        ) as f:
            writer = csv.writer(f, delimiter=";")
            writer.writerow([
                "date",
                "time",
                "serial",
                "tariff",
                "bluetooth",
                "volume_m3",
                "temperature_c",
                "battery_percent",
                "firmware",
                "rssi"
            ])


def save_reading(reading, device, rssi):
    values = reading.values

    title = reading.title.lower()

    if "тариф 1" in title:
        tariff = "1"
    elif "тариф 2" in title:
        tariff = "2"
    else:
        return

    volume = values.get("volume")
    temperature = values.get("temperature")
    battery = values.get("battery")

    key = (reading.serial, tariff)

    current = (
        volume,
        temperature,
        battery,
        reading.firmware
    )

    # Одинаковые показания повторно не записываем
    if last_values.get(key) == current:
        return

    try:
        now = datetime.now()

        with open(
            CSV_FILE,
            "a",
            newline="",
            encoding="utf-8-sig"
        ) as f:

            writer = csv.writer(f, delimiter=";")

            writer.writerow([
                now.strftime("%Y-%m-%d"),
                now.strftime("%H:%M:%S"),
                reading.serial,
                tariff,
                device.address,
                volume,
                temperature,
                battery,
                reading.firmware,
                rssi
            ])

        last_values[key] = current

        print(f"  [CSV] записано -> {CSV_FILE}")

    except PermissionError:
        print()
        print("  !!! CSV ЗАБЛОКИРОВАН !!!")
        print(f"  Файл: {CSV_FILE}")
        print("  Закройте CSV/Excel.")
        print("  Bluetooth продолжает работать.")
        print()


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
    print(f"Название  : {reading.title}")
    print(f"Серийный  : {reading.serial}")
    print(f"Версия    : {reading.packet_version}")
    print(f"Прошивка  : {reading.firmware}")

    for key, value in reading.values.items():
        print(f"{key:20} = {value}")

    save_reading(
        reading,
        device,
        advertisement_data.rssi
    )

    print("=" * 70)


async def main():

    ensure_csv()

    print("=" * 70)
    print(" ELEHANT SVT-15 MONITOR")
    print(" Серийный номер: 12020")
    print(" Тариф 1 + Тариф 2")
    print()
    print("История:")
    print(CSV_FILE)
    print()
    print("Bluetooth работает в режиме пассивного приёма.")
    print("К счётчику подключение НЕ выполняется.")
    print()
    print("Для остановки нажмите Ctrl+C")
    print("=" * 70)

    scanner = BleakScanner(
        detection_callback=callback
    )

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
