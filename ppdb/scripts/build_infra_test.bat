@echo off
setlocal enabledelayedexpansion

REM Set paths
set ROOT_DIR=%~dp0..
set SRC_DIR=%ROOT_DIR%\src
set TEST_DIR=%ROOT_DIR%\test\white\infra
set BUILD_DIR=%ROOT_DIR%\build\test
set CROSS9_DIR=%ROOT_DIR%\..\repos\cross9
set COSMO_DIR=%ROOT_DIR%\..\repos\cosmopolitan

REM Create build directory if not exists
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM Set compiler flags
set CFLAGS=-g -O2 -static -fno-pie -no-pie -mno-red-zone -nostdlib -nostdinc -fno-omit-frame-pointer -pg -mnop-mcount -I"%COSMO_DIR%" -I"%ROOT_DIR%\src"

REM Compile infra source files
echo Building infra source files...
"%CROSS9_DIR%\bin\x86_64-pc-linux-gnu-gcc.exe" %CFLAGS% -c "%SRC_DIR%\infra\infra_core.c" -o "%BUILD_DIR%\infra_core.o"
if errorlevel 1 exit /b 1

"%CROSS9_DIR%\bin\x86_64-pc-linux-gnu-gcc.exe" %CFLAGS% -c "%SRC_DIR%\infra\infra_struct.c" -o "%BUILD_DIR%\infra_struct.o"
if errorlevel 1 exit /b 1

REM Compile test files
echo Building test files...
"%CROSS9_DIR%\bin\x86_64-pc-linux-gnu-gcc.exe" %CFLAGS% -c "%TEST_DIR%\test_struct.c" -o "%BUILD_DIR%\test_struct.o"
if errorlevel 1 exit /b 1

REM Link test executable
echo Linking test executable...
"%CROSS9_DIR%\bin\x86_64-pc-linux-gnu-gcc.exe" %CFLAGS% -o "%BUILD_DIR%\test_struct.com.dbg" ^
    "%BUILD_DIR%\test_struct.o" ^
    "%BUILD_DIR%\infra_core.o" ^
    "%BUILD_DIR%\infra_struct.o" ^
    "%ROOT_DIR%\build\ape.o" ^
    "%ROOT_DIR%\build\cosmopolitan.a"
if errorlevel 1 exit /b 1

REM Strip debug info
"%CROSS9_DIR%\bin\x86_64-pc-linux-gnu-strip.exe" -o "%BUILD_DIR%\test_struct.com" "%BUILD_DIR%\test_struct.com.dbg"
if errorlevel 1 exit /b 1

REM Run tests
echo Running tests...
"%BUILD_DIR%\test_struct.com"
if errorlevel 1 (
    echo Test failed with error code %errorlevel%
    exit /b 1
)

echo Done. 