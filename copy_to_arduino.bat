@echo off
echo ============================================
echo  Copy HomeTempMonitor files to Arduino
echo ============================================
echo.

set SRC=D:\SEMCOWork\Session_12_HomeTemperature_ESP32\HomeTempMonitor
set DST=D:\Arduino_Project\HomeTempMonitor

if not exist "%DST%" (
    echo ERROR: Destination folder not found: %DST%
    echo Please check the path.
    pause
    exit /b 1
)

echo Source:      %SRC%
echo Destination: %DST%
echo.

echo Copying files...
copy /Y "%SRC%\HomeTempMonitor.ino" "%DST%\"
copy /Y "%SRC%\DataLogger.h"       "%DST%\"
copy /Y "%SRC%\DataLogger.cpp"     "%DST%\"
copy /Y "%SRC%\RTClib_DS3231.h"    "%DST%\"
copy /Y "%SRC%\RTClib_DS3231.cpp"  "%DST%\"
copy /Y "%SRC%\WebServer.h"        "%DST%\"
copy /Y "%SRC%\WebServer.cpp"      "%DST%\"

echo.
echo Done! All source files copied.
echo Open Arduino IDE and compile again.
pause
