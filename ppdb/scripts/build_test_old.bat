@echo off
setlocal enabledelayedexpansion

REM ==================== Version Info ====================
set VERSION=1.0.0
set BUILD_TYPE=Debug
set BUILD_TIME=%date% %time%

REM ==================== Command Line Args ====================
set CLEAN_ONLY=0
set BUILD_ONLY=0
set SHOW_HELP=0
set FORCE_REBUILD=0

:arg_loop
if "%1"=="" goto arg_done
if /i "%1"=="--clean" set CLEAN_ONLY=1
if /i "%1"=="--build-only" set BUILD_ONLY=1
if /i "%1"=="--help" set SHOW_HELP=1
if /i "%1"=="--rebuild" set FORCE_REBUILD=1
shift
goto arg_loop
:arg_done

if %SHOW_HELP%==1 (
    echo Usage: %~nx0 [options]
    echo Options:
    echo   --clean      Clean build directory only
    echo   --build-only Build without running tests
    echo   --help       Show this help message
    echo   --rebuild    Force rebuild all files
    exit /b 0
)

REM ==================== Environment Check ====================
echo PPDB Test Build v%VERSION% (%BUILD_TYPE%)
echo Build time: %BUILD_TIME%
echo.
echo Checking environment...

REM Set base directories
set R=%~dp0
cd/d %R%
set ROOT_DIR=%R%..
set CROSS_DIR=%ROOT_DIR%\cross9\bin
set COSMO_DIR=%ROOT_DIR%\cosmopolitan
set CC=%CROSS_DIR%\x86_64-pc-linux-gnu-gcc.exe
set AR=%CROSS_DIR%\x86_64-pc-linux-gnu-ar.exe
set OBJCOPY=%CROSS_DIR%\x86_64-pc-linux-gnu-objcopy.exe

REM Check required tools
if not exist "%CC%" (
    echo Error: Compiler not found at %CC%
    goto :error
)
if not exist "%AR%" (
    echo Error: AR not found at %AR%
    goto :error
)
if not exist "%OBJCOPY%" (
    echo Error: OBJCOPY not found at %OBJCOPY%
    goto :error
)

REM Check required files
if not exist "%COSMO_DIR%\cosmopolitan.h" (
    echo Error: cosmopolitan.h not found
    goto :error
)
if not exist "%COSMO_DIR%\ape.lds" (
    echo Error: ape.lds not found
    goto :error
)
if not exist "%COSMO_DIR%\crt.o" (
    echo Error: crt.o not found
    goto :error
)
if not exist "%COSMO_DIR%\ape-no-modify-self.o" (
    echo Error: ape-no-modify-self.o not found
    goto :error
)
if not exist "%COSMO_DIR%\cosmopolitan.a" (
    echo Error: cosmopolitan.a not found
    goto :error
)

REM ==================== Setup Build Flags ====================
echo Setting up build flags...

REM Set compiler flags
set COMMON_FLAGS=-g -O0 -Wall -Wextra -DPPDB_DEBUG -DPPDB_TEST -DPPDB_VERSION=\"%VERSION%\" -fno-pie -no-pie -mno-red-zone -fno-omit-frame-pointer -nostdlib -nostdinc -D_XOPEN_SOURCE=700
set WARNING_FLAGS=-Wno-sign-compare -Wno-unused-parameter -Wno-format-truncation
set INCLUDES=-I"%COSMO_DIR%" -I"%ROOT_DIR%" -I"%ROOT_DIR%\include" -I"%ROOT_DIR%\src" -I"%ROOT_DIR%\test_white" -include "%COSMO_DIR%\cosmopolitan.h"
set LDFLAGS=-Wl,--gc-sections -Wl,-z,max-page-size=0x1000 -fuse-ld=bfd -Wl,-T,"%COSMO_DIR%\ape.lds"

REM Set source directories
set SRC_DIR=%ROOT_DIR%\src
set TEST_DIR=%ROOT_DIR%\test_white
set BUILD_DIR=%ROOT_DIR%\build\test

REM ==================== Prepare Build ====================
echo Preparing build directory...

REM Clean and create output directory
if %CLEAN_ONLY%==1 (
    if exist "%BUILD_DIR%" (
        echo Cleaning build directory...
        rd /s /q "%BUILD_DIR%" 2>nul
    )
    echo Clean completed
    exit /b 0
)

if not exist "%BUILD_DIR%" (
    echo Creating build directory...
    mkdir "%BUILD_DIR%"
)

REM ==================== Build Source Files ====================
echo Building source files...

echo Compiling common files...
for %%f in ("%SRC_DIR%\common\*.c") do (
    set "SOURCE_FILE=%%f"
    set "OBJ_FILE=%BUILD_DIR%\common_%%~nf.o"
    
    REM Check if object file exists and source is newer
    set "NEED_COMPILE=0"
    if not exist "!OBJ_FILE!" set NEED_COMPILE=1
    if %FORCE_REBUILD%==1 set NEED_COMPILE=1
    if !NEED_COMPILE!==0 (
        for /f "tokens=*" %%t in ('forfiles /p "%SRC_DIR%\common" /m "%%~nxf" /c "cmd /c echo @fdate @ftime"') do set SRC_TIME=%%t
        if exist "!OBJ_FILE!" (
            for /f "tokens=*" %%t in ('forfiles /p "%BUILD_DIR%" /m "common_%%~nf.o" /c "cmd /c echo @fdate @ftime"') do set OBJ_TIME=%%t
            if "!SRC_TIME!" gtr "!OBJ_TIME!" set NEED_COMPILE=1
        ) else (
            set NEED_COMPILE=1
        )
    )
    
    if !NEED_COMPILE!==1 (
        echo   %%~nxf
        %CC% %COMMON_FLAGS% %WARNING_FLAGS% %INCLUDES% -c "%%f" -o "!OBJ_FILE!"
        if !errorlevel! neq 0 goto :error
    ) else (
        echo   %%~nxf [Skipped - up to date]
    )
)

echo Compiling kvstore files...
for %%f in ("%SRC_DIR%\kvstore\*.c") do (
    set "SOURCE_FILE=%%f"
    set "OBJ_FILE=%BUILD_DIR%\kvstore_%%~nf.o"
    
    REM Check if object file exists and source is newer
    set "NEED_COMPILE=0"
    if not exist "!OBJ_FILE!" set NEED_COMPILE=1
    if %FORCE_REBUILD%==1 set NEED_COMPILE=1
    if !NEED_COMPILE!==0 (
        for /f "tokens=*" %%t in ('forfiles /p "%SRC_DIR%\kvstore" /m "%%~nxf" /c "cmd /c echo @fdate @ftime"') do set SRC_TIME=%%t
        if exist "!OBJ_FILE!" (
            for /f "tokens=*" %%t in ('forfiles /p "%BUILD_DIR%" /m "kvstore_%%~nf.o" /c "cmd /c echo @fdate @ftime"') do set OBJ_TIME=%%t
            if "!SRC_TIME!" gtr "!OBJ_TIME!" set NEED_COMPILE=1
        ) else (
            set NEED_COMPILE=1
        )
    )
    
    if !NEED_COMPILE!==1 (
        echo   %%~nxf
        %CC% %COMMON_FLAGS% %WARNING_FLAGS% %INCLUDES% -c "%%f" -o "!OBJ_FILE!"
        if !errorlevel! neq 0 goto :error
    ) else (
        echo   %%~nxf [Skipped - up to date]
    )
)

REM ==================== Build Test Files ====================
echo Building test files...

echo Compiling test framework...
set "SOURCE_FILE=%TEST_DIR%\test_framework.c"
set "OBJ_FILE=%BUILD_DIR%\test_framework.o"

REM Check if object file exists and source is newer
set "NEED_COMPILE=0"
if not exist "!OBJ_FILE!" set NEED_COMPILE=1
if %FORCE_REBUILD%==1 set NEED_COMPILE=1
if !NEED_COMPILE!==0 (
    for /f "tokens=*" %%t in ('forfiles /p "%TEST_DIR%" /m "test_framework.c" /c "cmd /c echo @fdate @ftime"') do set SRC_TIME=%%t
    if exist "!OBJ_FILE!" (
        for /f "tokens=*" %%t in ('forfiles /p "%BUILD_DIR%" /m "test_framework.o" /c "cmd /c echo @fdate @ftime"') do set OBJ_TIME=%%t
        if "!SRC_TIME!" gtr "!OBJ_TIME!" set NEED_COMPILE=1
    ) else (
        set NEED_COMPILE=1
    )
)

if !NEED_COMPILE!==1 (
    echo   test_framework.c
    %CC% %COMMON_FLAGS% %WARNING_FLAGS% %INCLUDES% -c "!SOURCE_FILE!" -o "!OBJ_FILE!"
    if !errorlevel! neq 0 goto :error
) else (
    echo   test_framework.c [Skipped - up to date]
)

REM Build WAL test
echo Building WAL test...
%CC% %COMMON_FLAGS% %WARNING_FLAGS% %INCLUDES% %LDFLAGS% -o "%BUILD_DIR%\test_wal.dbg" ^
    "%TEST_DIR%\test_wal_main.c" ^
    "%TEST_DIR%\test_wal.c" ^
    "%TEST_DIR%\test_framework.c" ^
    "%SRC_DIR%\kvstore\wal.c" ^
    "%SRC_DIR%\kvstore\memtable.c" ^
    "%SRC_DIR%\kvstore\skiplist.c" ^
    "%SRC_DIR%\common\fs.c" ^
    "%SRC_DIR%\common\logger.c" ^
    "%COSMO_DIR%\crt.o" ^
    "%COSMO_DIR%\ape-no-modify-self.o" ^
    "%COSMO_DIR%\cosmopolitan.a"

if !errorlevel! neq 0 goto :error

echo Creating WAL test binary...
%OBJCOPY% -S -O binary "%BUILD_DIR%\test_wal.dbg" "%BUILD_DIR%\test_wal.exe"
if !errorlevel! neq 0 goto :error

REM Build MemTable test
echo Building MemTable test...
%CC% %COMMON_FLAGS% %WARNING_FLAGS% %INCLUDES% %LDFLAGS% -o "%BUILD_DIR%\test_memtable.dbg" ^
    "%TEST_DIR%\test_memtable_main.c" ^
    "%TEST_DIR%\test_memtable.c" ^
    "%TEST_DIR%\test_framework.c" ^
    "%SRC_DIR%\kvstore\memtable.c" ^
    "%SRC_DIR%\kvstore\skiplist.c" ^
    "%SRC_DIR%\common\fs.c" ^
    "%SRC_DIR%\common\logger.c" ^
    "%COSMO_DIR%\crt.o" ^
    "%COSMO_DIR%\ape-no-modify-self.o" ^
    "%COSMO_DIR%\cosmopolitan.a"

if !errorlevel! neq 0 goto :error

echo Creating MemTable test binary...
%OBJCOPY% -S -O binary "%BUILD_DIR%\test_memtable.dbg" "%BUILD_DIR%\test_memtable.exe"
if !errorlevel! neq 0 goto :error

REM Build KVStore test
echo Building KVStore test...
%CC% %COMMON_FLAGS% %WARNING_FLAGS% %INCLUDES% %LDFLAGS% -o "%BUILD_DIR%\test_kvstore.dbg" ^
    "%TEST_DIR%\test_kvstore_main.c" ^
    "%TEST_DIR%\test_kvstore.c" ^
    "%TEST_DIR%\test_framework.c" ^
    "%SRC_DIR%\kvstore\kvstore.c" ^
    "%SRC_DIR%\kvstore\memtable.c" ^
    "%SRC_DIR%\kvstore\skiplist.c" ^
    "%SRC_DIR%\kvstore\wal.c" ^
    "%SRC_DIR%\common\fs.c" ^
    "%SRC_DIR%\common\logger.c" ^
    "%COSMO_DIR%\crt.o" ^
    "%COSMO_DIR%\ape-no-modify-self.o" ^
    "%COSMO_DIR%\cosmopolitan.a"

if !errorlevel! neq 0 goto :error

echo Creating KVStore test binary...
%OBJCOPY% -S -O binary "%BUILD_DIR%\test_kvstore.dbg" "%BUILD_DIR%\test_kvstore.exe"
if !errorlevel! neq 0 goto :error

REM ==================== Run Tests ====================
if %BUILD_ONLY%==1 (
    echo Build completed successfully
    exit /b 0
)

echo Running tests...

REM Run WAL test
echo Running WAL test...
"%BUILD_DIR%\test_wal.exe"
set WAL_RESULT=%errorlevel%
if !WAL_RESULT! neq 0 (
    echo WAL test failed with error code !WAL_RESULT!
    goto :error
)
echo WAL tests completed with result: !WAL_RESULT!

REM Run MemTable test
echo Running MemTable test...
"%BUILD_DIR%\test_memtable.exe"
set MEMTABLE_RESULT=%errorlevel%
if !MEMTABLE_RESULT! neq 0 (
    echo MemTable test failed with error code !MEMTABLE_RESULT!
    goto :error
)
echo MemTable tests completed with result: !MEMTABLE_RESULT!

REM Run KVStore test
echo Running KVStore test...
"%BUILD_DIR%\test_kvstore.exe"
set KVSTORE_RESULT=%errorlevel%
if !KVSTORE_RESULT! neq 0 (
    echo KVStore test failed with error code !KVSTORE_RESULT!
    goto :error
)
echo KVStore tests completed with result: !KVSTORE_RESULT!

echo All tests completed successfully
exit /b 0

:error
echo.
echo Build failed with error code %errorlevel%
echo Current directory: %CD%
echo Build directory: %BUILD_DIR%
exit /b %errorlevel% 
