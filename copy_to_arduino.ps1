# copy_to_arduino.ps1
# 실행 방법: 우클릭 → PowerShell에서 실행
# 또는 PowerShell 열고: & "D:\SEMCOWork\Session_12_HomeTemperature_ESP32\HomeTempMonitor\copy_to_arduino.ps1"

$src = "D:\SEMCOWork\Session_12_HomeTemperature_ESP32\HomeTempMonitor"
$dst = "D:\Arduino_Project\HomeTempMonitor"

$files = @(
    "HomeTempMonitor.ino",
    "DataLogger.h",
    "DataLogger.cpp",
    "RTClib_DS3231.h",
    "RTClib_DS3231.cpp",
    "WebServer.h",
    "WebServer.cpp"
)

Write-Host "============================================" -ForegroundColor Cyan
Write-Host " Copy HomeTempMonitor to Arduino Project" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Source:      $src"
Write-Host "Destination: $dst"
Write-Host ""

if (-not (Test-Path $dst)) {
    Write-Host "ERROR: Destination not found: $dst" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

foreach ($f in $files) {
    $s = Join-Path $src $f
    $d = Join-Path $dst $f
    if (Test-Path $s) {
        Copy-Item $s $d -Force
        Write-Host "  OK: $f" -ForegroundColor Green
    } else {
        Write-Host "  MISSING: $f" -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "Done! Open Arduino IDE and compile again." -ForegroundColor Yellow
Read-Host "Press Enter to exit"
