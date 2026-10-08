@echo off
setlocal

echo ======================================
echo        ELEHANT SVT-15 MONITOR
echo ======================================
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1"

if errorlevel 1 (
    echo.
    echo INSTALLATION FAILED.
    pause
    exit /b 1
)

echo.
echo ======================================
echo        CONFIGURING AUTOSTART
echo ======================================
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0create_task.ps1"

if errorlevel 1 (
    echo.
    echo AUTOSTART CONFIGURATION FAILED.
    pause
    exit /b 1
)

echo.
echo ======================================
echo        INSTALLATION COMPLETE
echo ======================================
echo.

pause
endlocal
