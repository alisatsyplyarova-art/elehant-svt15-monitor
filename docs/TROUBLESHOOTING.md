# Elehant SVT-15 Monitor Troubleshooting

## 1. Meter is not detected

Check that Bluetooth is enabled in Windows.

The monitor uses Bluetooth Low Energy (BLE) advertising.
The meter does not need to be paired with Windows.

Move the computer closer to the meter and make sure the Bluetooth adapter is working.

## 2. Meter is detected but no readings appear

Check the background service:

    Get-Process Elehant_SVT15_Service -ErrorAction SilentlyContinue

If the process is not running, start the scheduled task:

    Start-ScheduledTask -TaskName "Elehant SVT-15 Monitor"

Wait several seconds and check the history file.

## 3. CSV is not updated

Check the history file:

    Get-Item C:\Elehant\elehant_history.csv

Show the latest records:

    Get-Content C:\Elehant\elehant_history.csv -Tail 10

If the file does not exist, check that the service is running.

## 4. GUI is empty

Start the GUI manually:

    C:\Elehant\Elehant_SVT15_Monitor.exe

Then check the history file:

    Get-Content C:\Elehant\elehant_history.csv -Tail 10

The GUI uses the same history file created by the background service.

## 5. Autostart does not work

Check the scheduled task:

    Get-ScheduledTask -TaskName "Elehant SVT-15 Monitor"

Check the last execution result:

    Get-ScheduledTaskInfo -TaskName "Elehant SVT-15 Monitor"

Start it manually:

    Start-ScheduledTask -TaskName "Elehant SVT-15 Monitor"

Then check the service process:

    Get-Process Elehant_SVT15_Service -ErrorAction SilentlyContinue

## 6. Service is running twice

PyInstaller one-file applications can show two processes for one running application.
A parent bootloader process and its child process are normal.

Do not stop one of the two processes only because two entries are visible.

## 7. Service cannot be updated

If the installer reports that Elehant_SVT15_Service.exe is in use, stop the running service first:

    Get-CimInstance Win32_Process | Where-Object {$_.ExecutablePath -eq "C:\Elehant\Elehant_SVT15_Service.exe"} | Select-Object ProcessId,Name,ExecutablePath

Then stop the reported processes and run the installer again.

## 8. Check the service manually

Run:

    C:\Elehant\Elehant_SVT15_Service.exe

The console should show BLE advertisements and decoded SVT-15 readings.

Stop it with Ctrl+C after testing.

## 9. History file location

The normal installed history file is:

    C:\Elehant\elehant_history.csv

The source-build version writes the CSV next to the executable.

## 10. Source development

For source-level diagnostics, install Python 3.14.x and bleak 3.0.2.
The source utilities are located in src\.

Build the Windows EXE files with:

    .\scripts\build_exe.ps1

## 11. Bluetooth

The monitor uses passive BLE advertising.
No pairing and no active connection to the meter are required.
