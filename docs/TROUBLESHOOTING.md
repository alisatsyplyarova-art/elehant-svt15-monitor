# Диагностика СВТ-15 Монитора

## 1. Счётчик не находится

Проверить Bluetooth:

Параметры Windows → Bluetooth и устройства → Bluetooth → Вкл.

Проверить BLE:

    C:\Elehant\.venv\Scripts\python.exe C:\Elehant\src\scan_ble.py

Если счётчик не обнаруживается, убедитесь, что компьютер находится рядом со счётчиком и Bluetooth-адаптер работает.

## 2. Счётчик находится, но показаний нет

Запустить:

    C:\Elehant\.venv\Scripts\python.exe C:\Elehant\src\elehant_read.py

У счётчика должна присутствовать manufacturer data с ID 0xFFFF.

## 3. Не создаётся CSV

Проверить каталог:

    C:\Elehant

Запустить сборщик вручную:

    C:\Elehant\.venv\Scripts\python.exe C:\Elehant\src\elehant_monitor.py

История должна появиться здесь:

    C:\Elehant\elehant_history.csv

## 4. Монитор пустой

Проверить последние записи:

    Get-Content C:\Elehant\elehant_history.csv -Tail 10

Если файл не обновляется, сначала проверить работу BLE-сборщика.

## 5. Не работает автозапуск

Открыть:

    taskschd.msc

Проверить задачу:

    Elehant SVT-15 Monitor

Также можно проверить:

    Get-ScheduledTask -TaskName "Elehant SVT-15 Monitor"

Запустить вручную:

    Start-ScheduledTask -TaskName "Elehant SVT-15 Monitor"

## 6. Проверка процесса

PowerShell:

    Get-Process python -ErrorAction SilentlyContinue

## 7. Проверка истории

PowerShell:

    Get-Content C:\Elehant\elehant_history.csv -Tail 10

## 8. Повторный запуск монитора

Перед ручным запуском убедитесь, что старый экземпляр монитора не работает. Иначе можно получить несколько одновременно работающих BLE-сборщиков.
