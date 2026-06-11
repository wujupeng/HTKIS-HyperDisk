@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1

set WDK_ROOT=C:\Program Files (x86)\Windows Kits\10
set WDK_VER=10.0.26100.0
set WDK_INC=%WDK_ROOT%\include\%WDK_VER%
set WDK_LIB=%WDK_ROOT%\lib\%WDK_VER%
set MSVC_LIB=%VCToolsInstallDir%\lib\x64

set SRC_DIR=D:\DEV\HTKIS-HyperDisk\driver\blk
set BUILD_DIR=D:\DEV\HTKIS-HyperDisk\driver\build

cl.exe /nologo /c /Zp8 /GS- /kernel /O2 /W3 /WX- ^
  /D_AMD64_ /D_WIN32 /D_WIN64 /D_NTKERNEL_ /DNTDDI_VERSION=0x0A000000 ^
  /I"%WDK_INC%\km" /I"%WDK_INC%\shared" /I"%WDK_INC%\um" ^
  /Fo"%BUILD_DIR%\storport_driver.obj" ^
  "%SRC_DIR%\storport_driver.c"

if %ERRORLEVEL% NEQ 0 (
  echo COMPILE FAILED
  exit /b 1
)

link.exe /nologo /kernel /driver /base:0x10000 /align:0x1000 /filealign:0x1000 ^
  /subsystem:native /entry:DriverEntry ^
  /libpath:"%WDK_LIB%\km\x64" /libpath:"%MSVC_LIB%" ^
  /out:"%BUILD_DIR%\HDBlk.sys" ^
  "%BUILD_DIR%\storport_driver.obj" ^
  storport.lib ntoskrnl.lib hal.lib BufferOverflowK.lib

if %ERRORLEVEL% NEQ 0 (
  echo LINK FAILED
  exit /b 1
)

echo BUILD SUCCESS
