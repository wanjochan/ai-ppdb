@echo off
setlocal EnableDelayedExpansion

rem Set paths
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"
set "ROOT_DIR=%CD%"
set "BUILD_DIR=%ROOT_DIR%\build"
set "COSMO=%ROOT_DIR%\..\cosmopolitan"
set "CROSS9=%ROOT_DIR%\..\cross9\bin"
set "GCC=%CROSS9%\x86_64-pc-linux-gnu-gcc.exe"
set "AR=%CROSS9%\x86_64-pc-linux-gnu-ar.exe"
set "OBJCOPY=%CROSS9%\x86_64-pc-linux-gnu-objcopy.exe"

rem Set flags
set "COMMON_FLAGS=-g -Os -static -fno-pie -no-pie -mno-red-zone -nostdlib -nostdinc -fno-omit-frame-pointer"
set "INCLUDE_FLAGS=-I%COSMO%"
set "LDFLAGS=-static -nostdlib -Wl,-T,%ROOT_DIR%\..\ppdb\build\ape.lds -Wl,--gc-sections -fuse-ld=bfd -Wl,-z,max-page-size=0x1000 -no-pie"
set "LIBS=%ROOT_DIR%\..\ppdb\build\crt.o %ROOT_DIR%\..\ppdb\build\ape.o %ROOT_DIR%\..\ppdb\build\cosmopolitan.a"

rem Create build directory if not exists
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem Build cosmo.exe
echo Building cosmo.exe...
"%GCC%" %COMMON_FLAGS% %INCLUDE_FLAGS% -o "%BUILD_DIR%\cosmo.o" -c cosmo.c

"%GCC%" %COMMON_FLAGS% -o "%BUILD_DIR%\cosmo.com.dbg" %LDFLAGS% ^
    "%BUILD_DIR%\cosmo.o" %LIBS%

"%OBJCOPY%" -S -O binary "%BUILD_DIR%\cosmo.com.dbg" "%BUILD_DIR%\cosmo.com"
copy /b "%BUILD_DIR%\cosmo.com" "%ROOT_DIR%\cosmo.exe"

rem Build test module
echo Building test module...
"%GCC%" %COMMON_FLAGS% -fPIC -c main.c -o "%BUILD_DIR%\main.o" %INCLUDE_FLAGS%
"%GCC%" %COMMON_FLAGS% -shared -o "%ROOT_DIR%\main.dat" "%BUILD_DIR%\main.o"

echo Build complete.
