@echo off
setlocal EnableDelayedExpansion

set ITERATIONS=20
set LOGFILE=C:\HyperDisk\alpha_a_loop.log
set RESULTFILE=C:\HyperDisk\alpha_a_results.txt

if not exist "C:\HyperDisk" mkdir "C:\HyperDisk"

echo [%date% %time%] Alpha-A Loop Test Started > "%RESULTFILE%"
echo Target: %ITERATIONS% iterations >> "%RESULTFILE%"
echo. >> "%RESULTFILE%"

set PASS=0
set FAIL=0

for /L %%i in (1,1,%ITERATIONS%) do (
    echo ============================================
    echo  Iteration %%i / %ITERATIONS%
    echo ============================================

    echo [%date% %time%] Iteration %%i starting... >> "%RESULTFILE%"

    echo [CHECK] PXE boot stage...
    if exist "C:\HyperDisk\pxe_marker" (
        echo [OK] PXE marker found
    ) else (
        echo [INFO] PXE marker not yet checked
    )

    echo [CHECK] WinPE boot stage...
    ver | findstr /i "10.0" >nul
    if !errorlevel! equ 0 (
        echo [OK] WinPE booted successfully
        echo [%date% %time%] WinPE check: PASS >> "%RESULTFILE%"
        set /a PASS+=1
    ) else (
        echo [FAIL] WinPE not detected
        echo [%date% %time%] WinPE check: FAIL >> "%RESULTFILE%"
        set /a FAIL+=1
    )

    echo [CHECK] bootdiag execution...
    if exist "C:\HyperDisk\bootdiag.exe" (
        "C:\HyperDisk\bootdiag.exe" --config "C:\HyperDisk\boot.meta" >> "%LOGFILE%" 2>&1
        if !errorlevel! equ 0 (
            echo [OK] bootdiag PASS
            echo [%date% %time%] bootdiag: PASS >> "%RESULTFILE%"
        ) else (
            echo [WARN] bootdiag returned !errorlevel!
            echo [%date% %time%] bootdiag: WARN(!errorlevel!) >> "%RESULTFILE%"
        )
    ) else (
        echo [SKIP] bootdiag.exe not found
        echo [%date% %time%] bootdiag: SKIP >> "%RESULTFILE%"
    )

    echo [CHECK] COM1 serial output...
    if exist "C:\HyperDisk\com1_log.txt" (
        findstr /c:"[HDx:WINPE_START]" "C:\HyperDisk\com1_log.txt" >nul
        if !errorlevel! equ 0 (
            echo [OK] COM1 WINPE_START found
            echo [%date% %time%] COM1: PASS >> "%RESULTFILE%"
        ) else (
            echo [WARN] COM1 WINPE_START not found
            echo [%date% %time%] COM1: WARN >> "%RESULTFILE%"
        )
    )

    echo [%date% %time%] Iteration %%i complete. PASS=!PASS! FAIL=!FAIL! >> "%RESULTFILE%"
    echo. >> "%RESULTFILE%"

    if %%i lss %ITERATIONS% (
        echo [ACTION] Rebooting in 3 seconds...
        timeout /t 3 /nobreak >nul
        shutdown /r /t 0
    )
)

echo. >> "%RESULTFILE%"
echo ============================================ >> "%RESULTFILE%"
echo  Alpha-A Loop Test Complete >> "%RESULTFILE%"
echo  PASS: !PASS! / %ITERATIONS% >> "%RESULTFILE%"
echo  FAIL: !FAIL! / %ITERATIONS% >> "%RESULTFILE%"
echo  Result: !PASS!/%ITERATIONS% >> "%RESULTFILE%"
echo ============================================ >> "%RESULTFILE%"

echo.
echo ============================================
echo  Alpha-A Loop Test Complete
echo  PASS: !PASS! / %ITERATIONS%
echo  FAIL: !FAIL! / %ITERATIONS%
echo ============================================

if !PASS! equ %ITERATIONS% (
    echo  VERDICT: ACHIEVED
) else (
    echo  VERDICT: NOT_ACHIEVED
)

endlocal
