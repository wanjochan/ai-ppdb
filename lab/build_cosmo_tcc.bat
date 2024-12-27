@echo off
chcp 65001 > nul
setlocal enabledelayedexpansion

REM ==================== Environment Setup ====================
echo Setting up environment...

REM Set base directories
set R=%~dp0
set ROOT_DIR=d:\dev\ai-ppdb
set PPDB_DIR=%ROOT_DIR%\ppdb
set COSMO_DIR=%PPDB_DIR%\cosmopolitan
set CROSS_DIR=%PPDB_DIR%\cross9\bin
set TCC_DIR=%ROOT_DIR%\cosmo_tinycc
set LAB_DIR=%ROOT_DIR%\lab

REM Set toolchain
set CC=%CROSS_DIR%\x86_64-pc-linux-gnu-gcc.exe
set AR=%CROSS_DIR%\x86_64-pc-linux-gnu-ar.exe
set RANLIB=%CROSS_DIR%\x86_64-pc-linux-gnu-ranlib.exe
set LD=%CROSS_DIR%\x86_64-pc-linux-gnu-ld.exe

REM Check tools and files
if not exist "%CC%" (
    echo Error: Compiler not found at %CC%
    exit /b 1
)
if not exist "%COSMO_DIR%\cosmopolitan.h" (
    echo Error: cosmopolitan.h not found
    exit /b 1
)
if not exist "%COSMO_DIR%\ape.lds" (
    echo Error: ape.lds not found
    exit /b 1
)
if not exist "%COSMO_DIR%\crt.o" (
    echo Error: crt.o not found
    exit /b 1
)
if not exist "%COSMO_DIR%\ape-copy-self.o" (
    echo Error: ape-copy-self.o not found
    exit /b 1
)
if not exist "%COSMO_DIR%\cosmopolitan.a" (
    echo Error: cosmopolitan.a not found
    exit /b 1
)

REM Set build flags
set COMMON_FLAGS=-g -Os -Wall -fno-pie -no-pie -mno-red-zone -fno-omit-frame-pointer -nostdlib -nostdinc
set WARNING_FLAGS=-Wno-sign-compare -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable
set INCLUDES=-I"%COSMO_DIR%" -I"%TCC_DIR%" -I"%LAB_DIR%" -I"%COSMO_DIR%\libc\nexgen32e" -I"%COSMO_DIR%\libc\isystem"
set CFLAGS=%COMMON_FLAGS% %WARNING_FLAGS% %INCLUDES% -DCONFIG_COSMO -ffunction-sections -fdata-sections -include "%COSMO_DIR%\cosmopolitan.h"
set LDFLAGS=-static -nostdlib -T "%COSMO_DIR%\ape.lds" -Wl,--gc-sections -Wl,-z,max-page-size=0x1000

REM ==================== Build Steps ====================
echo Building TinyCC...

REM Prepare build directory
cd /d %TCC_DIR%

REM Copy config file
copy /y "%LAB_DIR%\config-cosmo.h" "%TCC_DIR%\config.h" >nul

REM Build core files
echo Building core files...
set CORE_FILES=tcc.c libtcc.c tccpp.c tccgen.c x86_64-gen.c tccelf.c tccape.c tccrun.c tccasm.c tccdbg.c tcctools.c

for %%f in (%CORE_FILES%) do (
    echo   Compiling %%f...
    "%CC%" %CFLAGS% -c %%f -o %%~nf.o
    if errorlevel 1 (
        echo Error: Failed to compile %%f
        exit /b 1
    )
)

REM Link
echo Linking...
"%LD%" %LDFLAGS% -o tcc.exe ^
    "%COSMO_DIR%\ape-copy-self.o" ^
    "%COSMO_DIR%\crt.o" ^
    tcc.o libtcc.o tccpp.o tccgen.o x86_64-gen.o ^
    tccelf.o tccape.o tccrun.o tccasm.o tccdbg.o ^
    tcctools.o ^
    "%COSMO_DIR%\cosmopolitan.a"

if errorlevel 1 (
    echo Error: Linking failed
    exit /b 1
)

echo Build successful!
exit /b 0
