<#
.SYNOPSIS
    Restores the Safe Version of ATS-Mini v2.38 (with 2.5s hold to cancel) to the ESP32-S3 radio.
#>

$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   Restore Safe Version (ATS-Mini v2.38 + Hold-Cancel)   " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

$backupPath = Join-Path $PSScriptRoot "safe_v2.38_hold_back_16MB.bin"

if (-not (Test-Path $backupPath)) {
    Write-Error "Safe backup file not found at: $backupPath"
}

# Auto-detect COM port if not specified
$comPort = $args[0]
if (-not $comPort) {
    $espDevice = Get-CimInstance Win32_PnPEntity | Where-Object { $_.DeviceID -match "VID_303A&PID_1001" } | Select-Object -First 1
    if ($espDevice -and ($espDevice.Name -match "COM(\d+)")) {
        $comPort = "COM$($Matches[1])"
        Write-Host "Auto-detected ESP32-S3 on port: $comPort" -ForegroundColor Green
    } else {
        $comPort = Read-Host "Enter COM port (e.g. COM11)"
    }
}

Write-Host "`nTarget file: $backupPath" -ForegroundColor Yellow
Write-Host "Target port: $comPort" -ForegroundColor Yellow
Write-Host "Speed:       921600 baud" -ForegroundColor Yellow

uvx esptool -p $comPort -b 921600 write_flash 0x0 $backupPath

Write-Host "`nRestore complete! Device has been returned to the Safe Version." -ForegroundColor Green
