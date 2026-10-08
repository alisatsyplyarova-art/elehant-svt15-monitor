$ErrorActionPreference = "Stop"

$Root = Split-Path $PSScriptRoot -Parent
$Venv = Join-Path $Root ".venv"
$Python = Join-Path $Venv "Scripts\python.exe"
$Release = Join-Path $Root "release"
$BuildGui = Join-Path $Root "build-gui"
$BuildService = Join-Path $Root "build-service"

$GuiSource = Join-Path $Root "src\elehant_viewer.py"
$ServiceSource = Join-Path $Root "src\elehant_monitor.py"
$ParserDir = Join-Path $Root "third_party\elehant_water\custom_components\elehant_water"

$GuiExe = Join-Path $Release "Elehant_SVT15_Monitor.exe"
$ServiceExe = Join-Path $Release "Elehant_SVT15_Service.exe"

if (-not (Test-Path $GuiSource)) {
    throw "Не найден GUI: $GuiSource"
}

if (-not (Test-Path $ServiceSource)) {
    throw "Не найден сервис: $ServiceSource"
}

if (-not (Test-Path $ParserDir)) {
    throw "Не найден каталог parser: $ParserDir"
}

if (-not (Test-Path $Python)) {
    Write-Host "Создание виртуального окружения..." -ForegroundColor Cyan
    python -m venv $Venv
}

if (-not (Test-Path $Python)) {
    throw "Не удалось создать виртуальное окружение: $Venv"
}

Set-Location $Root

Write-Host "Установка PyInstaller..." -ForegroundColor Cyan
& $Python -m pip install --upgrade pip
& $Python -m pip install pyinstaller bleak==3.0.2

if (Test-Path $Release) {
    Remove-Item $Release -Recurse -Force
}

if (Test-Path $BuildGui) {
    Remove-Item $BuildGui -Recurse -Force
}

if (Test-Path $BuildService) {
    Remove-Item $BuildService -Recurse -Force
}

New-Item -ItemType Directory -Path $Release -Force | Out-Null

Write-Host ""
Write-Host "======================================" -ForegroundColor Cyan
Write-Host "       СБОРКА GUI"
Write-Host "======================================" -ForegroundColor Cyan
Write-Host ""

& $Python -m PyInstaller `
    --onefile `
    --windowed `
    --clean `
    --noconfirm `
    --name "Elehant_SVT15_Monitor" `
    --distpath $Release `
    --workpath $BuildGui `
    $GuiSource

if (-not (Test-Path $GuiExe)) {
    throw "GUI EXE не создан: $GuiExe"
}

Write-Host ""
Write-Host "======================================" -ForegroundColor Cyan
Write-Host "       СБОРКА СЕРВИСА"
Write-Host "======================================" -ForegroundColor Cyan
Write-Host ""

& $Python -m PyInstaller `
    --onefile `
    --clean `
    --noconfirm `
    --name "Elehant_SVT15_Service" `
    --paths $ParserDir `
    --distpath $Release `
    --workpath $BuildService `
    $ServiceSource

if (-not (Test-Path $ServiceExe)) {
    throw "Service EXE не создан: $ServiceExe"
}

Write-Host ""
Write-Host "======================================" -ForegroundColor Green
Write-Host "       СБОРКА ЗАВЕРШЕНА"
Write-Host "======================================" -ForegroundColor Green
Write-Host ""
Write-Host "GUI:"
Write-Host "  $GuiExe"
Write-Host ""
Write-Host "Service:"
Write-Host "  $ServiceExe"
Write-Host ""
Get-ChildItem $Release -Filter "*.exe" | Select-Object Name,Length
