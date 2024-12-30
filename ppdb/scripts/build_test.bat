@echo off
setlocal EnableDelayedExpansion

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
set "OBJCOPY=%CROSS9%\x86_64-pc-linux-gnu-objcopy.exe"
set "TEST_DIR=%ROOT_DIR%\test"

rem Get test type
set "TEST_TYPE=%1"
if "%TEST_TYPE%"=="" set "TEST_TYPE=all"

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

if not exist "%OBJCOPY%" (
    echo Error: OBJCOPY not found
    exit /b 1
)

rem Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem Set compilation flags
set "COMMON_FLAGS=-Wall -Wextra -fno-pie -fno-stack-protector -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -fno-asynchronous-unwind-tables"
set "DEBUG_FLAGS=-g -O0 -DDEBUG"

if "%TEST_TYPE%"=="42" (
    echo Building %TEST_TYPE% test...
    rem Simple configuration for %TEST_TYPE% test
    set "CFLAGS=!COMMON_FLAGS! !DEBUG_FLAGS! -nostdinc -I!ROOT_DIR! -I!ROOT_DIR!\include -I!ROOT_DIR!\src -I!COSMO! -I!TEST_DIR!\white -include !COSMO!\cosmopolitan.h"
    set "LDFLAGS=-static -nostdlib -Wl,-T,!COSMO!\ape.lds -Wl,--gc-sections -fuse-ld=bfd -Wl,-z,max-page-size=0x1000 -no-pie"
    set "LIBS=!COSMO!\crt.o !COSMO!\ape.o !COSMO!\cosmopolitan.a"
    
    echo Compiling with flags: !CFLAGS!
    echo Linking with flags: !LDFLAGS!
    echo Libraries: !LIBS!
    
    echo Building %TEST_TYPE% test...
    "!GCC!" !CFLAGS! ^
        test/white/test_%TEST_TYPE%.c ^
        !LDFLAGS! !LIBS! ^
        -o "!BUILD_DIR!\%TEST_TYPE%_test.dbg"
    if errorlevel 1 (
        echo Error: Test build failed
        exit /b 1
    )

    echo Converting to APE format...
    echo Command: "!OBJCOPY!" -S -O binary "!BUILD_DIR!\%TEST_TYPE%_test.dbg" "!BUILD_DIR!\%TEST_TYPE%_test.exe"
    "!OBJCOPY!" -S -O binary "!BUILD_DIR!\%TEST_TYPE%_test.dbg" "!BUILD_DIR!\%TEST_TYPE%_test.exe"
    if errorlevel 1 (
        echo Error: objcopy failed
        exit /b 1
    )

    echo Running %TEST_TYPE% test...
    echo Command: "!BUILD_DIR!\%TEST_TYPE%_test.exe"
    "!BUILD_DIR!\%TEST_TYPE%_test.exe"
    echo Exit code: !errorlevel!
) else (
    rem Original configuration for other tests
    set "CFLAGS=%COMMON_FLAGS% %DEBUG_FLAGS% -I%INCLUDE_DIR% -I%COSMO% -I%ROOT_DIR%/src -include %COSMO%\cosmopolitan.h"
    set "LDFLAGS=-static -nostdlib -Wl,-T,%COSMO%\ape.lds -Wl,--gc-sections -fuse-ld=bfd -Wl,-z,max-page-size=0x1000 -no-pie"
    set "LIBS=%COSMO%\crt.o %COSMO%\ape.o %BUILD_DIR%\libppdb.a %COSMO%\cosmopolitan.a"

    if "%TEST_TYPE%"=="unit" (
        echo Building unit tests...
        "%GCC%" %CFLAGS% ^
            src/kvstore/memtable.c ^
            src/kvstore/skiplist.c ^
            src/kvstore/sync.c ^
            test/white/test_basic.c ^
            test/white/test_framework.c ^
            %LDFLAGS% %LIBS% ^
            -o "%BUILD_DIR%\unit_test.exe"
        if errorlevel 1 (
            echo Error: Test build failed
            exit /b 1
        )
        echo Running unit tests...
        "%BUILD_DIR%\unit_test.exe"
    ) else if "%TEST_TYPE%"=="all" (
        echo Building all tests...
        "%GCC%" %CFLAGS% ^
            src/kvstore/memtable.c ^
            src/kvstore/skiplist.c ^
            src/kvstore/sync.c ^
            test/white/test_basic.c ^
            test/white/test_sync.c ^
            test/white/test_42.c ^
            test/white/test_framework.c ^
            %LDFLAGS% %LIBS% ^
            -o "%BUILD_DIR%\all_test.exe"
        if errorlevel 1 (
            echo Error: Test build failed
            exit /b 1
        )
        echo Running all tests...
        "%BUILD_DIR%\all_test.exe"
    ) else (
        echo Error: Unknown test type '%TEST_TYPE%'
        echo Usage: build_test.bat [unit^|42^|all]
        exit /b 1
    )
)

echo Cleaning up...
endlocal
