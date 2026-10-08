$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$InstallDir = "C:\Elehant"

$MonitorExe = Join-Path $ProjectRoot "release\Elehant_SVT15_Monitor.exe"
$ServiceExe = Join-Path $ProjectRoot "release\Elehant_SVT15_Service.exe"
$VbsSource  = Join-Path $ProjectRoot "install\start_elephant_hidden.vbs"

Write-Host ""
Write-Host "======================================"
Write-Host "Installation"
Write-Host "======================================"
Write-Host ""

if (-not (Test-Path $MonitorExe)) {
    throw "Required installer file is missing: $MonitorExe / $ServiceExe / $VbsSource"
}

if (-not (Test-Path $ServiceExe)) {
    throw "Required installer file is missing: $MonitorExe / $ServiceExe / $VbsSource"
}

if (-not (Test-Path $VbsSource)) {
    throw "Required installer file is missing: $MonitorExe / $ServiceExe / $VbsSource"
}

Write-Host "OK" -ForegroundColor Green

Write-Host ""
Write-Host "Working..." -ForegroundColor Cyan

New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null

Write-Host ""
Write-Host "Working..." -ForegroundColor Cyan

Copy-Item `
    $MonitorExe `
    "$InstallDir\Elehant_SVT15_Monitor.exe" `
    -Force

Write-Host ""
Write-Host "Working..." -ForegroundColor Cyan

Copy-Item `
    $ServiceExe `
    "$InstallDir\Elehant_SVT15_Service.exe" `
    -Force

Write-Host ""
Write-Host "Working..." -ForegroundColor Cyan

Copy-Item `
    $VbsSource `
    "$InstallDir\start_elephant_hidden.vbs" `
    -Force

Write-Host ""
Write-Host "Working..." -ForegroundColor Cyan

$InstalledFiles = @(
    "$InstallDir\Elehant_SVT15_Monitor.exe",
    "$InstallDir\Elehant_SVT15_Service.exe",
    "$InstallDir\start_elephant_hidden.vbs"
)

foreach ($File in $InstalledFiles) {
    if (-not (Test-Path $File)) {
    throw "Required installer file is missing: $MonitorExe / $ServiceExe / $VbsSource"
    }

    Write-Host "OK  $File" -ForegroundColor Green
}

Write-Host ""
Write-Host "======================================"
Write-Host "Installation"
Write-Host "======================================"
Write-Host ""

Write-Host "GUI:"
Write-Host "  $InstallDir\Elehant_SVT15_Monitor.exe"

Write-Host ""
Write-Host "Installation"
Write-Host "  $InstallDir\Elehant_SVT15_Service.exe"

Write-Host ""
Write-Host "Installation"
Write-Host "  $InstallDir\elehant_history.csv"

Write-Host ""
Write-Host "Installation"
Write-Host "  $InstallDir\start_elephant_hidden.vbs"

Write-Host ""
Write-Host "OK" -ForegroundColor Green
Write-Host ""

