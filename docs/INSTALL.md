# Установка СВТ-15 Монитора

## 1. Требования

- Windows 10 или Windows 11
- Bluetooth-адаптер с поддержкой Bluetooth Low Energy (BLE)
- Python 3.14.x

Счётчик не требуется сопрягать обычным способом через настройки Bluetooth Windows.

## 2. Установка

Самый простой способ — использовать готовый установщик.

Откройте каталог:

    install

и запустите:

    install.bat

Установщик проверяет Python, создаёт C:\Elehant, копирует программу и BLE-парсер, создаёт виртуальное окружение, устанавливает Bleak и создаёт задачу автозапуска Windows.

## 3. Каталог установки

После установки программа находится в:

    C:\Elehant

Основные файлы:

    C:\Elehant\src\elehant_monitor.py
    C:\Elehant\src\elehant_read.py
    C:\Elehant\src\elehant_viewer.py
    C:\Elehant\src\scan_ble.py
    C:\Elehant\src\listen_ble.py
    C:\Elehant\third_party\elehant_water
    C:\Elehant\elehant_history.csv
    C:\Elehant\start_elephant_hidden.vbs

## 4. Проверка BLE

Проверить обнаружение счётчика:

    C:\Elehant\.venv\Scripts\python.exe C:\Elehant\src\scan_ble.py

Проверить получение показаний:

    C:\Elehant\.venv\Scripts\python.exe C:\Elehant\src\elehant_read.py

## 5. Сбор истории

Основной BLE-сборщик:

    C:\Elehant\.venv\Scripts\python.exe C:\Elehant\src\elehant_monitor.py

История сохраняется в:

    C:\Elehant\elehant_history.csv

## 6. Графический монитор

Запуск:

    C:\Elehant\.venv\Scripts\python.exe C:\Elehant\src\elehant_viewer.py

Также можно использовать готовый Windows EXE из GitHub Releases.

## 7. Автозапуск

Установщик создаёт задачу Планировщика Windows:

    Elehant SVT-15 Monitor

Запуск выполняется при входе пользователя в Windows.

Используется файл:

    C:\Elehant\start_elephant_hidden.vbs

Монитор запускается скрыто, без окна консоли.

## 8. Проверка задачи

    Get-ScheduledTask -TaskName "Elehant SVT-15 Monitor"

Запустить вручную:

    Start-ScheduledTask -TaskName "Elehant SVT-15 Monitor"

Проверить процесс:

    Get-Process python -ErrorAction SilentlyContinue

## 9. История показаний

    Get-Content C:\Elehant\elehant_history.csv -Tail 10

Для СВТ-15:

- тариф 1 — ГОРЯЧАЯ вода;
- тариф 2 — ХОЛОДНАЯ вода.

## 10. Удаление автозапуска

    Unregister-ScheduledTask -TaskName "Elehant SVT-15 Monitor" -Confirm:$false

После удаления задачи каталог C:\Elehant можно удалить вручную.

## 11. Важное замечание

Счётчик передаёт показания через BLE advertising.

Подключаться к счётчику через обычное меню Bluetooth Windows не требуется.

Компьютер должен находиться в зоне радиосвязи Bluetooth с установленным счётчиком.
