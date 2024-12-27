@echo off
chcp 65001 > nul
setlocal enabledelayedexpansion

REM ==================== Environment Setup ====================
set R=%~dp0
set ROOT_DIR=%R%
set PPDB_DIR=%R%..\ppdb
set CROSS_DIR=%PPDB_DIR%\cross9\bin
set COSMO_DIR=%PPDB_DIR%\cosmopolitan

set CC=%CROSS_DIR%\x86_64-pc-linux-gnu-gcc.exe
set AR=%CROSS_DIR%\x86_64-pc-linux-gnu-ar.exe
set OBJCOPY=%CROSS_DIR%\x86_64-pc-linux-gnu-objcopy.exe

REM Check tools and files
if not exist "%CC%" (
    echo Error: Compiler not found at %CC%
    goto :error
)
if not exist "%COSMO_DIR%\cosmopolitan.h" (
    echo Error: cosmopolitan.h not found
    goto :error
)

REM ==================== Build Flags ====================
set COMMON_FLAGS=-g -O0 -Wall -Wextra -fno-pie -no-pie -mno-red-zone -fno-omit-frame-pointer -nostdlib -nostdinc -D_XOPEN_SOURCE=700 -DTCC_TARGET_X86_64 -DONE_SOURCE=1
set INCLUDES=-I"%COSMO_DIR%" -I"%ROOT_DIR%\tinycc" -include "%COSMO_DIR%\cosmopolitan.h"
set LDFLAGS=-static -nostdlib -Wl,--gc-sections -Wl,-z,max-page-size=0x1000 -fuse-ld=bfd -Wl,-T,"%COSMO_DIR%\ape.lds" -Wl,--start-group "%COSMO_DIR%\cosmopolitan.a" -Wl,--end-group

REM ==================== Build Steps ====================
echo Building cosmo_run...

REM Compile TinyCC core files
%CC% %COMMON_FLAGS% %INCLUDES% -c "%ROOT_DIR%\tinycc\libtcc.c" -o "%ROOT_DIR%\libtcc.o"
if errorlevel 1 goto error

%CC% %COMMON_FLAGS% %INCLUDES% -c "%ROOT_DIR%\tinycc\tccgen.c" -o "%ROOT_DIR%\tccgen.o"
if errorlevel 1 goto error

%CC% %COMMON_FLAGS% %INCLUDES% -c "%ROOT_DIR%\tinycc\tccpp.c" -o "%ROOT_DIR%\tccpp.o"
if errorlevel 1 goto error

%CC% %COMMON_FLAGS% %INCLUDES% -c "%ROOT_DIR%\tinycc\tccelf.c" -o "%ROOT_DIR%\tccelf.o"
if errorlevel 1 goto error

REM Compile cosmo_run.c
%CC% %COMMON_FLAGS% %INCLUDES% -c "%ROOT_DIR%\cosmo_run.c" -o "%ROOT_DIR%\cosmo_run.o"
if errorlevel 1 goto error

REM Link
%CC% -o "%ROOT_DIR%\cosmo_run.dbg" "%ROOT_DIR%\cosmo_run.o" "%ROOT_DIR%\libtcc.o" "%ROOT_DIR%\tccgen.o" "%ROOT_DIR%\tccpp.o" "%ROOT_DIR%\tccelf.o" "%COSMO_DIR%\crt.o" "%COSMO_DIR%\ape-no-modify-self.o" %LDFLAGS%
if errorlevel 1 goto error

REM Create binary and copy to .exe
%OBJCOPY% -S -O binary "%ROOT_DIR%\cosmo_run.dbg" "%ROOT_DIR%\cosmo_run.bin"
copy /y "%ROOT_DIR%\cosmo_run.bin" "%ROOT_DIR%\cosmo_run.exe" >nul
if errorlevel 1 goto error

echo Build successful!
exit /b 0

:error
echo Build failed!
exit /b 1
