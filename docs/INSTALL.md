# Elehant SVT-15 Monitor Installation

## 1. Requirements

For the ready-to-use Windows EXE version:
- Windows 10 or Windows 11
- Bluetooth adapter with Bluetooth Low Energy (BLE) support
- Administrator rights for installation

Python is NOT required for the ready-to-use version.

The meter does not need to be paired through Windows Bluetooth settings.
The program receives SVT-15 data through BLE advertising and does not connect to the meter.

## 2. Installing the ready-to-use version

1. Download the latest Windows release from GitHub Releases.
2. Extract the project directory.
3. Open the project directory as Administrator.
4. Run:

    install\install.bat

The installer:
- checks the required EXE files;
- creates C:\Elehant;
- installs the GUI monitor;
- installs the background BLE service;
- installs the VBS autostart file;
- creates the Windows Scheduled Task.

Python, virtual environments and Bleak are NOT installed by the ready-to-use installer.

## 3. Installation directory

    C:\Elehant

Main files:

    C:\Elehant\Elehant_SVT15_Monitor.exe
    C:\Elehant\Elehant_SVT15_Service.exe
    C:\Elehant\start_elephant_hidden.vbs
    C:\Elehant\elehant_history.csv

## 4. Check the background service

Check the process:

    Get-Process Elehant_SVT15_Service -ErrorAction SilentlyContinue

The service listens for BLE advertisements from SVT-15 meters.

## 5. Check the history

The history file is:

    C:\Elehant\elehant_history.csv

Show the latest records:

    Get-Content C:\Elehant\elehant_history.csv -Tail 10

## 6. Start the GUI

Run:

    C:\Elehant\Elehant_SVT15_Monitor.exe

The GUI displays current meter readings and historical data.

## 7. Autostart

The installer creates the Windows Scheduled Task:

    Elehant SVT-15 Monitor

The task starts the background service when the user logs in.

The VBS launcher is:

    C:\Elehant\start_elephant_hidden.vbs

## 8. Check the Scheduled Task

    Get-ScheduledTask -TaskName "Elehant SVT-15 Monitor"

Show the last task result:

    Get-ScheduledTaskInfo -TaskName "Elehant SVT-15 Monitor"

## 9. Remove autostart

To remove the scheduled task:

    Unregister-ScheduledTask -TaskName "Elehant SVT-15 Monitor" -Confirm:$false

The installed files in C:\Elehant can then be removed manually.

## 10. Building from source

Source development requires:
- Python 3.14.x
- bleak 3.0.2
- PyInstaller

Build both Windows EXE files with:

    .\scripts\build_exe.ps1

The resulting files are placed in:

    release\Elehant_SVT15_Monitor.exe
    release\Elehant_SVT15_Service.exe

## 11. Bluetooth

The monitor uses passive BLE advertising.
No pairing and no active connection to the meter are required.
