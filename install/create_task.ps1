$ErrorActionPreference = "Stop"

$TaskName = "Elehant SVT-15 Monitor"
$Vbs = "C:\Elehant\start_elephant_hidden.vbs"

if (-not (Test-Path $Vbs)) {
    Write-Host "Не найден $Vbs" -ForegroundColor Red
    exit 1
}

$Action = New-ScheduledTaskAction `
    -Execute "wscript.exe" `
    -Argument "`"$Vbs`""

$Trigger = New-ScheduledTaskTrigger -AtLogOn

$Principal = New-ScheduledTaskPrincipal `
    -UserId "$env:USERDOMAIN\$env:USERNAME" `
    -LogonType Interactive `
    -RunLevel Limited

$Settings = New-ScheduledTaskSettingsSet `
    -AllowStartIfOnBatteries `
    -DontStopIfGoingOnBatteries `
    -ExecutionTimeLimit (New-TimeSpan -Days 3)

Register-ScheduledTask `
    -TaskName $TaskName `
    -Action $Action `
    -Trigger $Trigger `
    -Principal $Principal `
    -Settings $Settings `
    -Force

Write-Host ""
Write-Host "Задача создана:" -ForegroundColor Green
Write-Host $TaskName

Write-Host ""
Write-Host "Проверка:"
Get-ScheduledTask -TaskName $TaskName | Format-List TaskName,State
