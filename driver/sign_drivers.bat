@echo off
setlocal

set WDK_ROOT=C:\Program Files (x86)\Windows Kits\10
set WDK_VER=10.0.26100.0
set WDK_BIN=%WDK_ROOT%\bin\%WDK_VER%\x64
set DRIVER_ROOT=%~dp0
set OUTDIR=%DRIVER_ROOT%build

echo ============================================
echo  HyperDisk Driver Test Signing
echo ============================================
echo.

if not exist "%WDK_BIN%\makecat.exe" (
    echo [ERROR] WDK tools not found. Is WDK installed?
    goto :end
)

if not exist "%OUTDIR%\HDBus.sys" (
    echo [ERROR] No .sys files found. Run build_drivers.bat first.
    goto :end
)

echo Creating catalog files and test-signing drivers...
echo.

for %%d in (HDBus HDBlk HDNet HyperCache HyperOverlay) do (
    if exist "%OUTDIR%\%%d.sys" (
        echo [SIGN] %%d.sys
        "%WDK_BIN%\inf2cat" /driver:%OUTDIR% /os:10_X64
        "%WDK_BIN%\signtool" sign /fd SHA256 /a /t http://timestamp.digicert.com /td SHA256 "%OUTDIR%\%%d.sys"
    )
)

echo.
echo ============================================
echo  Signing complete
echo ============================================

:end
endlocal
