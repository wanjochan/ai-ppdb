@echo off
setlocal

rem Set paths
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%\.."
set "ROOT_DIR=%CD%"
set "BUILD_DIR=%ROOT_DIR%\build"
set "INCLUDE_DIR=%ROOT_DIR%\include"
set "COSMO=%ROOT_DIR%\..\cosmopolitan"
set "CROSS9=%ROOT_DIR%\..\cross9\bin"
set "GCC=%CROSS9%\x86_64-pc-linux-gnu-gcc.exe"
set "AR=%CROSS9%\x86_64-pc-linux-gnu-ar.exe"
set "TEST_DIR=%ROOT_DIR%\test"

rem Get test type
set "TEST_TYPE=%1"
if "%TEST_TYPE%"=="" set "TEST_TYPE=all"

rem Set test environment
set "TEST_ENV="
if "%TEST_TYPE%"=="unit" set "TEST_ENV=set TEST_TYPE=unit &&"
if "%TEST_TYPE%"=="concurrent" set "TEST_ENV=set TEST_TYPE=concurrent &&"
if "%TEST_TYPE%"=="edge" set "TEST_ENV=set TEST_TYPE=edge &&"
if "%TEST_TYPE%"=="stress" set "TEST_ENV=set TEST_TYPE=stress &&"
if "%TEST_TYPE%"=="recovery" set "TEST_ENV=set TEST_TYPE=recovery &&"
if "%TEST_TYPE%"=="integration" set "TEST_ENV=set TEST_TYPE=integration &&"

rem Check directories
if not exist "%COSMO%" (
    echo Error: Cosmopolitan directory not found
    exit /b 1
)

if not exist "%CROSS9%" (
    echo Error: Cross9 directory not found
    exit /b 1
)

if not exist "%GCC%" (
    echo Error: GCC not found
    exit /b 1
)

if not exist "%AR%" (
    echo Error: AR not found
    exit /b 1
)

rem Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem Set compilation flags
set "COMMON_FLAGS=-Wall -Wextra -fno-pie -fno-stack-protector -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -fno-asynchronous-unwind-tables"
set "DEBUG_FLAGS=-g -O0 -DDEBUG"
set "CFLAGS=%COMMON_FLAGS% %DEBUG_FLAGS% -I%INCLUDE_DIR% -I%COSMO% -I%ROOT_DIR%/src -include %COSMO%/cosmopolitan.h"

rem Set linker flags
set "LDFLAGS=-static -nostdlib -Wl,-T,%COSMO%\ape.lds -Wl,--gc-sections -fuse-ld=bfd -Wl,-z,max-page-size=0x1000 -no-pie"
set "LIBS=%COSMO%\crt.o %COSMO%\ape.o %BUILD_DIR%\libppdb.a %COSMO%\cosmopolitan.a"

rem Build library if needed
if not exist "%BUILD_DIR%\libppdb.a" (
    echo Building library...
    call "%SCRIPT_DIR%\build_ppdb.bat"
    if errorlevel 1 (
        echo Error: Library build failed
        exit /b 1
    )
)

rem Set test sources
if "%TEST_TYPE%"=="unit" (
    set "TEST_SOURCES=%TEST_DIR%\white\test_framework.c %TEST_DIR%\white\test_basic.c"
    set "TEST_TARGET=unit_test.exe"
) else if "%TEST_TYPE%"=="concurrent" (
    set "TEST_SOURCES=%TEST_DIR%\white\test_framework.c %TEST_DIR%\white\test_concurrent.c"
    set "TEST_TARGET=concurrent_test.exe"
) else if "%TEST_TYPE%"=="edge" (
    set "TEST_SOURCES=%TEST_DIR%\white\test_framework.c %TEST_DIR%\white\test_edge.c"
    set "TEST_TARGET=edge_test.exe"
) else if "%TEST_TYPE%"=="stress" (
    set "TEST_SOURCES=%TEST_DIR%\white\test_framework.c %TEST_DIR%\white\test_stress.c"
    set "TEST_TARGET=stress_test.exe"
) else if "%TEST_TYPE%"=="recovery" (
    set "TEST_SOURCES=%TEST_DIR%\white\test_framework.c %TEST_DIR%\white\test_recovery.c"
    set "TEST_TARGET=recovery_test.exe"
) else if "%TEST_TYPE%"=="integration" (
    set "TEST_SOURCES=%TEST_DIR%\black\integration\*.c"
    set "TEST_TARGET=integration_test.exe"
) else (
    set "TEST_SOURCES=%TEST_DIR%\white\test_*.c"
    set "TEST_TARGET=all_test.exe"
)

rem Build tests
echo Building %TEST_TYPE% tests...
"%GCC%" %CFLAGS% %TEST_SOURCES% -o "%BUILD_DIR%\%TEST_TARGET%.dbg" %LDFLAGS% %LIBS%
"%CROSS9%\x86_64-pc-linux-gnu-objcopy.exe" -S -O binary "%BUILD_DIR%\%TEST_TARGET%.dbg" "%BUILD_DIR%\%TEST_TARGET%"

rem Run tests
if exist "%BUILD_DIR%\%TEST_TARGET%" (
    echo Running %TEST_TYPE% tests...
    %TEST_ENV% "%BUILD_DIR%\%TEST_TARGET%"
) else (
    echo Error: Test build failed
    exit /b 1
)

rem Clean up
echo Cleaning up...
del *.o 2>nul

exit /b 0
