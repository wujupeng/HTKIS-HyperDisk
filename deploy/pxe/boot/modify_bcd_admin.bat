@echo off
title HTKIS BCD TestSigning + Disable HVCI
echo ============================================
echo  HTKIS BCD TestSigning + Disable HVCI
echo ============================================
echo.

set BCD_SRC=D:\DEV\HTKIS-HyperDisk\deploy\pxe\boot\BCD_clean
set BCD_DST=D:\DEV\HTKIS-HyperDisk\deploy\pxe\boot\BCD_testsign
set LOADER={f39d4830-5cd8-11f1-890d-2088106aa670}

echo Step 1: Copy BCD_clean to BCD_testsign...
copy /Y "%BCD_SRC%" "%BCD_DST%"
echo.

echo Step 2: Enable testsigning on OS loader entry...
bcdedit /store "%BCD_DST%" /set %LOADER% testsigning on 2>&1
echo.

echo Step 3: Enable nointegritychecks on OS loader entry...
bcdedit /store "%BCD_DST%" /set %LOADER% nointegritychecks on 2>&1
echo.

echo Step 4: Disable HVCI (hypervisorlaunchtype off)...
bcdedit /store "%BCD_DST%" /set %LOADER% disableelamdrivers Yes 2>&1
echo.

echo Step 5: Set hypervisorlaunchtype off on bootmgr...
bcdedit /store "%BCD_DST%" /set {bootmgr} hypervisorlaunchtype off 2>&1
echo.

echo Step 6: Verify OS loader entry...
bcdedit /store "%BCD_DST%" /enum %LOADER% 2>&1
echo.

echo Step 7: Verify bootmgr entry...
bcdedit /store "%BCD_DST%" /enum {bootmgr} 2>&1
echo.

echo Step 8: Check file size...
for %%A in ("%BCD_DST%") do echo BCD_testsign: %%~zA bytes
echo.

echo ============================================
echo  Done!
echo ============================================
pause
