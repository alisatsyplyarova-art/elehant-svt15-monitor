$ErrorActionPreference = "Stop"

$Root = Split-Path $PSScriptRoot -Parent
$Source = Join-Path $Root "src\elehant_viewer.py"
$Venv = Join-Path $Root ".venv"
$Python = Join-Path $Venv "Scripts\python.exe"
$Release = Join-Path $Root "release"
$Exe = Join-Path $Release "СВТ-15 Монитор.exe"

if (-not (Test-Path $Source)) {
    throw "Не найден исходный файл: $Source"
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
& $Python -m pip install pyinstaller

if (Test-Path $Release) {
    Remove-Item $Release -Recurse -Force
}

New-Item -ItemType Directory -Path $Release -Force | Out-Null

Write-Host ""
Write-Host "Сборка EXE..." -ForegroundColor Cyan

& $Python -m PyInstaller `
    --onefile `
    --windowed `
    --clean `
    --noconfirm `
    --name "СВТ-15 Монитор" `
    --distpath $Release `
    --workpath (Join-Path $Root "build") `
    $Source

if (-not (Test-Path $Exe)) {
    throw "EXE не создан: $Exe"
}

Write-Host ""
Write-Host "======================================" -ForegroundColor Green
Write-Host "       EXE успешно создан" -ForegroundColor Green
Write-Host "======================================" -ForegroundColor Green
Write-Host ""
Write-Host $Exe
