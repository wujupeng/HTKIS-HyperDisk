@echo off
setlocal EnableDelayedExpansion

set WDK_ROOT=C:\Program Files (x86)\Windows Kits\10
set WDK_VER=10.0.26100.0
set MSVC_ROOT=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231

set PATH=%MSVC_ROOT%\bin\Hostx64\x64;%PATH%

set WDK_INC=%WDK_ROOT%\Include\%WDK_VER%
set WDK_LIB=%WDK_ROOT%\Lib\%WDK_VER%
set MSVC_INC=%MSVC_ROOT%\include
set MSVC_LIB=%MSVC_ROOT%\lib\x64

set CFLAGS=/kernel /GS- /GL- /MP /W3 /WX- /Zi /Od /Oi /Oy- /c /D_WIN32 /D_AMD64_ /D_KERNEL_MODE /DWIN32_LEAN_AND_MEAN=1 /D_NO_CRT_STDIO_EQUAL /DNTDDI_VERSION=0x0A000007 /DWINVER=0x0A00 /D_WIN32_WINNT=0x0A00 /DPOOL_ZEROING=1 /DPOOL_ZERO_DOWNLEVEL_SUPPORT=1
set INCLUDES=/I"%WDK_INC%\km" /I"%WDK_INC%\shared" /I"%WDK_INC%\ucrt" /I"%WDK_INC%\km\crt" /I"%MSVC_INC%"
set LFLAGS=/NODEFAULTLIB /SUBSYSTEM:NATIVE /DRIVER /KERNEL /ENTRY:DriverEntry /MERGE:.text=.PAGE /MERGE:.data=.PAGE /SECTION:INIT,d /SECTION:PAGE,d /IGNORE:4197 /IGNORE:4010 /IGNORE:4078 /IGNORE:4221 /DEBUG /LIBPATH:"%WDK_LIB%\km\x64" /LIBPATH:"%MSVC_LIB%"

set DRIVER_ROOT=%~dp0
set OUTDIR=%DRIVER_ROOT%build
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

echo ============================================
echo  HyperDisk WDK Driver Build System
echo  WDK: %WDK_VER%  Target: x64 KM
echo ============================================
echo.

if not exist "%WDK_INC%\km" (
    echo [ERROR] WDK km includes not found at %WDK_INC%\km
    echo  WDK may not be installed. Install WDK %WDK_VER% first.
    goto :end
)

set BUILD_OK=1
set DRV_COUNT=0

echo [1/6] Building hd_driver_common (static lib)...
if not exist "%OUTDIR%\common" mkdir "%OUTDIR%\common"
cl %CFLAGS% %INCLUDES% /Fo"%OUTDIR%\common\\" "%DRIVER_ROOT%common\hd_debug.c" "%DRIVER_ROOT%common\hd_memory.c" "%DRIVER_ROOT%common\hd_lock.c" "%DRIVER_ROOT%common\hd_registry.c" "%DRIVER_ROOT%common\hd_serial.c"
if errorlevel 1 (
    echo [FAIL] common build failed
    set BUILD_OK=0
    goto :end
)
lib /OUT:"%OUTDIR%\hd_driver_common.lib" "%OUTDIR%\common\hd_debug.obj" "%OUTDIR%\common\hd_memory.obj" "%OUTDIR%\common\hd_lock.obj" "%OUTDIR%\common\hd_registry.obj" "%OUTDIR%\common\hd_serial.obj"
echo [OK] hd_driver_common.lib
echo.

echo [2/6] Building HDBus.sys...
if not exist "%OUTDIR%\bus" mkdir "%OUTDIR%\bus"
cl %CFLAGS% %INCLUDES% /I"%DRIVER_ROOT%include" /I"%DRIVER_ROOT%common" /I"%DRIVER_ROOT%bus" /Fo"%OUTDIR%\bus\\" "%DRIVER_ROOT%bus\bus_driver.c" "%DRIVER_ROOT%bus\bus_pnp.c" "%DRIVER_ROOT%bus\bus_ioctl.c" "%DRIVER_ROOT%bus\bus_child.c"
if errorlevel 1 (
    echo [FAIL] HDBus compile failed
    set BUILD_OK=0
    goto :end
)
link %LFLAGS% /OUT:"%OUTDIR%\HDBus.sys" "%OUTDIR%\bus\bus_driver.obj" "%OUTDIR%\bus\bus_pnp.obj" "%OUTDIR%\bus\bus_ioctl.obj" "%OUTDIR%\bus\bus_child.obj" "%OUTDIR%\hd_driver_common.lib" ntoskrnl.lib hal.lib
if errorlevel 1 (
    echo [FAIL] HDBus.sys link failed
    set BUILD_OK=0
    goto :end
)
echo [OK] HDBus.sys
set /a DRV_COUNT+=1
echo.

echo [3/6] Building HDBlk.sys...
if not exist "%OUTDIR%\blk" mkdir "%OUTDIR%\blk"
cl %CFLAGS% %INCLUDES% /I"%DRIVER_ROOT%include" /I"%DRIVER_ROOT%common" /I"%DRIVER_ROOT%blk" /Fo"%OUTDIR%\blk\\" "%DRIVER_ROOT%blk\blk_driver.c" "%DRIVER_ROOT%blk\blk_io.c" "%DRIVER_ROOT%blk\blk_ioctl.c" "%DRIVER_ROOT%blk\blk_request.c" "%DRIVER_ROOT%blk\blk_irp_tracker.c" "%DRIVER_ROOT%blk\blk_fastio.c" "%DRIVER_ROOT%blk\blk_cc_mm_sync.c" "%DRIVER_ROOT%blk\blk_pool.c" "%DRIVER_ROOT%blk\blk_7b_recovery.c"
if errorlevel 1 (
    echo [FAIL] HDBlk compile failed
    set BUILD_OK=0
    goto :end
)
link %LFLAGS% /OUT:"%OUTDIR%\HDBlk.sys" "%OUTDIR%\blk\blk_driver.obj" "%OUTDIR%\blk\blk_io.obj" "%OUTDIR%\blk\blk_ioctl.obj" "%OUTDIR%\blk\blk_request.obj" "%OUTDIR%\blk\blk_irp_tracker.obj" "%OUTDIR%\blk\blk_fastio.obj" "%OUTDIR%\blk\blk_cc_mm_sync.obj" "%OUTDIR%\blk\blk_pool.obj" "%OUTDIR%\blk\blk_7b_recovery.obj" "%OUTDIR%\hd_driver_common.lib" ntoskrnl.lib hal.lib
if errorlevel 1 (
    echo [FAIL] HDBlk.sys link failed
    set BUILD_OK=0
    goto :end
)
echo [OK] HDBlk.sys
set /a DRV_COUNT+=1
echo.

echo [4/6] Building HDNet.sys...
if not exist "%OUTDIR%\net" mkdir "%OUTDIR%\net"
cl %CFLAGS% %INCLUDES% /I"%DRIVER_ROOT%include" /I"%DRIVER_ROOT%common" /I"%DRIVER_ROOT%net" /Fo"%OUTDIR%\net\\" "%DRIVER_ROOT%net\net_driver.c" "%DRIVER_ROOT%net\net_wsk.c" "%DRIVER_ROOT%net\net_frame.c" "%DRIVER_ROOT%net\net_connect.c" "%DRIVER_ROOT%net\net_heartbeat.c"
if errorlevel 1 (
    echo [FAIL] HDNet compile failed
    set BUILD_OK=0
    goto :end
)
link %LFLAGS% /OUT:"%OUTDIR%\HDNet.sys" "%OUTDIR%\net\net_driver.obj" "%OUTDIR%\net\net_wsk.obj" "%OUTDIR%\net\net_frame.obj" "%OUTDIR%\net\net_connect.obj" "%OUTDIR%\net\net_heartbeat.obj" "%OUTDIR%\hd_driver_common.lib" ntoskrnl.lib hal.lib netio.lib
if errorlevel 1 (
    echo [FAIL] HDNet.sys link failed
    set BUILD_OK=0
    goto :end
)
echo [OK] HDNet.sys
set /a DRV_COUNT+=1
echo.

echo [5/6] Building HyperCache.sys (MiniFilter altitude 380000)...
if not exist "%OUTDIR%\cache" mkdir "%OUTDIR%\cache"
cl %CFLAGS% %INCLUDES% /I"%DRIVER_ROOT%include" /I"%DRIVER_ROOT%common" /I"%DRIVER_ROOT%cache" /Fo"%OUTDIR%\cache\\" "%DRIVER_ROOT%cache\cache_entry.c" "%DRIVER_ROOT%cache\cache_driver.c"
if errorlevel 1 (
    echo [FAIL] HyperCache compile failed
    set BUILD_OK=0
    goto :end
)
link %LFLAGS% /OUT:"%OUTDIR%\HyperCache.sys" "%OUTDIR%\cache\cache_entry.obj" "%OUTDIR%\cache\cache_driver.obj" "%OUTDIR%\hd_driver_common.lib" ntoskrnl.lib hal.lib fltMgr.lib
if errorlevel 1 (
    echo [FAIL] HyperCache.sys link failed
    set BUILD_OK=0
    goto :end
)
echo [OK] HyperCache.sys
set /a DRV_COUNT+=1
echo.

echo [6/6] Building HyperOverlay.sys (MiniFilter altitude 390000)...
if not exist "%OUTDIR%\overlay" mkdir "%OUTDIR%\overlay"
cl %CFLAGS% %INCLUDES% /I"%DRIVER_ROOT%include" /I"%DRIVER_ROOT%common" /I"%DRIVER_ROOT%overlay" /Fo"%OUTDIR%\overlay\\" "%DRIVER_ROOT%overlay\overlay_entry.c" "%DRIVER_ROOT%overlay\overlay_driver.c"
if errorlevel 1 (
    echo [FAIL] HyperOverlay compile failed
    set BUILD_OK=0
    goto :end
)
link %LFLAGS% /OUT:"%OUTDIR%\HyperOverlay.sys" "%OUTDIR%\overlay\overlay_entry.obj" "%OUTDIR%\overlay\overlay_driver.obj" "%OUTDIR%\hd_driver_common.lib" ntoskrnl.lib hal.lib fltMgr.lib
if errorlevel 1 (
    echo [FAIL] HyperOverlay.sys link failed
    set BUILD_OK=0
    goto :end
)
echo [OK] HyperOverlay.sys
set /a DRV_COUNT+=1
echo.

echo ============================================
echo  Build Summary
echo ============================================
if "%BUILD_OK%"=="1" (
    echo  %DRV_COUNT% drivers built successfully!
    echo.
    for %%f in ("%OUTDIR%\*.sys") do (
        echo  [OK] %%~nxf  %%~zf bytes
    )
    echo.
    echo  Deliverables:
    for %%f in ("%OUTDIR%\*.sys" "%DRIVER_ROOT%bus\HyperDiskBus.inf" "%DRIVER_ROOT%blk\HyperDiskBlk.inf" "%DRIVER_ROOT%net\HyperNet.inf" "%DRIVER_ROOT%cache\HyperCache.inf" "%DRIVER_ROOT%overlay\HyperOverlay.inf") do (
        echo  %%~nxf
    )
) else (
    echo  BUILD FAILED - check errors above
)
echo ============================================

:end
endlocal
