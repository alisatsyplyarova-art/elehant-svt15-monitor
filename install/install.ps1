$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$InstallDir = "C:\Elehant"

Write-Host ""
Write-Host "======================================" -ForegroundColor Cyan
Write-Host "       Установка СВТ-15 Монитора" -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan
Write-Host ""

$PythonCommand = Get-Command python -ErrorAction SilentlyContinue

if (-not $PythonCommand) {
    Write-Host "Python не найден." -ForegroundColor Red
    Write-Host "Установите Python 3.14.x и повторите запуск."
    exit 1
}

New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
New-Item -ItemType Directory -Path "$InstallDir\src" -Force | Out-Null
New-Item -ItemType Directory -Path "$InstallDir\third_party" -Force | Out-Null

Write-Host "Копирование программы..." -ForegroundColor Cyan

Copy-Item `
    "$ProjectRoot\src\*" `
    "$InstallDir\src" `
    -Recurse `
    -Force

Write-Host "Копирование parser.py и third_party..." -ForegroundColor Cyan

Copy-Item `
    "$ProjectRoot\third_party\*" `
    "$InstallDir\third_party" `
    -Recurse `
    -Force

Write-Host "Копирование автозапуска..." -ForegroundColor Cyan

Copy-Item `
    "$ProjectRoot\install\start_elephant_hidden.vbs" `
    "$InstallDir\start_elephant_hidden.vbs" `
    -Force

Write-Host ""
Write-Host "Проверка виртуального окружения..." -ForegroundColor Cyan

$Python = "$InstallDir\.venv\Scripts\python.exe"

if (-not (Test-Path $Python)) {
    Write-Host "Создание виртуального окружения..."
    & python -m venv "$InstallDir\.venv"
}

if (-not (Test-Path $Python)) {
    throw "Не удалось создать виртуальное окружение."
}

Write-Host ""
Write-Host "Установка зависимостей..." -ForegroundColor Cyan

& $Python -m pip install --upgrade pip
& $Python -m pip install bleak==3.0.2

Write-Host ""
Write-Host "Проверка установки..." -ForegroundColor Cyan

& $Python --version
& $Python -c "from importlib.metadata import version; print('Bleak:', version('bleak'))"

$Parser = "$InstallDir\third_party\elehant_water\custom_components\elehant_water\parser.py"

if (-not (Test-Path $Parser)) {
    throw "parser.py не найден: $Parser"
}

$Monitor = "$InstallDir\src\elehant_monitor.py"

if (-not (Test-Path $Monitor)) {
    throw "elehant_monitor.py не найден: $Monitor"
}

$Viewer = "$InstallDir\src\elehant_viewer.py"

if (-not (Test-Path $Viewer)) {
    throw "elehant_viewer.py не найден: $Viewer"
}

$Vbs = "$InstallDir\start_elephant_hidden.vbs"

if (-not (Test-Path $Vbs)) {
    throw "start_elephant_hidden.vbs не найден: $Vbs"
}

Write-Host ""
Write-Host "Все основные файлы найдены." -ForegroundColor Green

Write-Host ""
Write-Host "======================================" -ForegroundColor Green
Write-Host "       Установка завершена" -ForegroundColor Green
Write-Host "======================================" -ForegroundColor Green
Write-Host ""
Write-Host "Программа:"
Write-Host "  $InstallDir\src"
Write-Host ""
Write-Host "Монитор:"
Write-Host "  $InstallDir\src\elehant_monitor.py"
Write-Host ""
Write-Host "GUI:"
Write-Host "  $InstallDir\src\elehant_viewer.py"
Write-Host ""
Write-Host "История:"
Write-Host "  $InstallDir\elehant_history.csv"
Write-Host ""
Write-Host "Автозапуск:"
Write-Host "  $InstallDir\start_elephant_hidden.vbs"
Write-Host ""


