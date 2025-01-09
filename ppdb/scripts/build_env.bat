@echo off
rem ===== Environment Variables and Common Functions =====

rem Set paths (using absolute paths)
set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%\.."
pushd "%ROOT_DIR%"
set "ROOT_DIR=%CD%"
popd
set "PPDB_DIR=%ROOT_DIR%"
set "BUILD_DIR=%PPDB_DIR%\build"
set "BIN_DIR=%PPDB_DIR%\bin"
set "SRC_DIR=%PPDB_DIR%\src"
set "INCLUDE_DIR=%PPDB_DIR%\include"
set "INTERNAL_DIR=%PPDB_DIR%\src\internal"
set "TEST_DIR=%PPDB_DIR%\test"

rem Set tool paths
set "COSMO=%ROOT_DIR%\..\repos\cosmopolitan"
set "CROSS9=%ROOT_DIR%\..\repos\cross9\bin"
set "GCC=%CROSS9%\x86_64-pc-linux-gnu-gcc.exe"
set "AR=%CROSS9%\x86_64-pc-linux-gnu-ar.exe"
set "OBJCOPY=%CROSS9%\x86_64-pc-linux-gnu-objcopy.exe"

rem Verify tool paths
if not exist "%GCC%" (
    echo Error: GCC not found at %GCC%
    exit /b 1
)
if not exist "%AR%" (
    echo Error: AR not found at %AR%
    exit /b 1
)
if not exist "%OBJCOPY%" (
    echo Error: OBJCOPY not found at %OBJCOPY%
    exit /b 1
)

rem Set build flags
set "COMMON_FLAGS=-g -O2 -Wall -Wextra -fno-pie -fno-stack-protector -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -fno-asynchronous-unwind-tables -ftls-model=initial-exec"
set "DEBUG_FLAGS=-DDEBUG"
set "BUILD_FLAGS=%COMMON_FLAGS% %DEBUG_FLAGS%"

rem Set include paths
set "INCLUDE_FLAGS=-nostdinc -I%PPDB_DIR% -I%PPDB_DIR%\include -I%PPDB_DIR%\src -I%PPDB_DIR%\src\internal -I%COSMO% -I%COSMO%\libc -I%COSMO%\libc\calls -I%COSMO%\libc\sock -I%COSMO%\libc\thread -I%TEST_DIR%\white -I%CROSS9%\..\x86_64-pc-linux-gnu\include"

rem Set final CFLAGS
set "CFLAGS=%BUILD_FLAGS% %INCLUDE_FLAGS% -include %COSMO%\cosmopolitan.h"

rem Set linker flags
set "LDFLAGS=-static -nostdlib -Wl,-T,%BUILD_DIR%\ape.lds -Wl,--gc-sections -B%CROSS9% -Wl,-z,max-page-size=0x1000 -no-pie"
set "LIBS=%BUILD_DIR%\crt.o %BUILD_DIR%\ape.o %BUILD_DIR%\cosmopolitan.a"

rem Check runtime files
if not exist "%BUILD_DIR%\crt.o" (
    echo Error: Missing runtime files. Please run setup.bat first
    exit /b 1
)
if not exist "%BUILD_DIR%\ape.o" (
    echo Error: Missing runtime files. Please run setup.bat first
    exit /b 1
)
if not exist "%BUILD_DIR%\cosmopolitan.a" (
    echo Error: Missing runtime files. Please run setup.bat first
    exit /b 1
)
if not exist "%BUILD_DIR%\ape.lds" (
    echo Error: Missing runtime files. Please run setup.bat first
    exit /b 1
)

exit /b 0