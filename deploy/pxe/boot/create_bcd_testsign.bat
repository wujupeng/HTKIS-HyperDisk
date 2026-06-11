@echo off
echo Creating BCD with testsigning enabled...
echo.

set BCD_SRC=D:\DEV\HTKIS-HyperDisk\deploy\pxe\boot\BCD_clean
set BCD_DST=D:\DEV\HTKIS-HyperDisk\deploy\pxe\boot\BCD_testsign

copy /Y "%BCD_SRC%" "%BCD_DST%"

reg load HKLM\BCD_TEMP "%BCD_DST%"
if %ERRORLEVEL% NEQ 0 (
    echo FAILED to load BCD hive. Run as Administrator!
    pause
    exit /b 1
)

echo.
echo Adding TestSigning element to OS loader...
reg add "HKLM\BCD_TEMP\Objects\{f39d4830-5cd8-11f1-890d-2088106aa670}\Elements\16000049" /ve /d 1 /t REG_DWORD /f

echo.
echo Verifying...
reg query "HKLM\BCD_TEMP\Objects\{f39d4830-5cd8-11f1-890d-2088106aa670}\Elements\16000049"

echo.
echo Unloading hive...
reg unload HKLM\BCD_TEMP

echo.
echo Done! BCD_testsign created with testsigning enabled.
pause
