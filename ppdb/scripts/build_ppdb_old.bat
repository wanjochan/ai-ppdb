@echo off
setlocal enabledelayedexpansion

REM ==================== Version Info ====================
set VERSION=1.0.0
set BUILD_TYPE=Release
set BUILD_TIME=%date% %time%

REM ==================== Command Line Args ====================
set CLEAN_ONLY=0
set SHOW_HELP=0
set FORCE_REBUILD=1

:arg_loop
if "%1"=="" goto arg_done
if /i "%1"=="--clean" set CLEAN_ONLY=1
if /i "%1"=="--help" set SHOW_HELP=1
if /i "%1"=="--no-rebuild" set FORCE_REBUILD=0
shift
goto arg_loop
:arg_done

if %SHOW_HELP%==1 (
    echo Usage: %~nx0 [options]
    echo Options:
    echo   --clean      Clean build directory only
    echo   --help       Show this help message
    echo   --rebuild    Force rebuild all files
    exit /b 0
)

REM ==================== Environment Check ====================
echo PPDB Library Build v%VERSION% (%BUILD_TYPE%)
echo Build time: %BUILD_TIME%
echo.
echo Checking environment...

REM Set base directories
set R=%~dp0
cd /d %R%
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
set COMMON_FLAGS=-O2 -Wall -Wextra -DNDEBUG -DPPDB_VERSION=\"%VERSION%\" -fno-pie -no-pie -mno-red-zone -fno-omit-frame-pointer -nostdlib -nostdinc -fPIC
set INCLUDES=-I"%COSMO_DIR%" -I"%ROOT_DIR%" -I"%ROOT_DIR%\include" -I"%ROOT_DIR%\src" -include "%COSMO_DIR%\cosmopolitan.h"
set LDFLAGS=-Wl,--gc-sections -Wl,-z,max-page-size=0x1000 -fuse-ld=bfd -Wl,-T,"%COSMO_DIR%\ape.lds"

REM Set source directories
set SRC_DIR=%ROOT_DIR%\src
set BUILD_DIR=%ROOT_DIR%\build\release

REM ==================== Prepare Build ====================
echo Preparing build directory...

REM Clean and create output directory
if %CLEAN_ONLY%==1 (
    if exist "%BUILD_DIR%" (
        echo Cleaning build directory...
        del /q "%BUILD_DIR%\*.*" 2>nul
    )
    echo Clean completed
    exit /b 0
)

if not exist "%BUILD_DIR%" (
    echo Creating build directory...
    mkdir "%BUILD_DIR%"
)
if not exist "%BUILD_DIR%\include\ppdb" mkdir "%BUILD_DIR%\include\ppdb"

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
        %CC% %COMMON_FLAGS% %INCLUDES% -c "%%f" -o "!OBJ_FILE!"
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
        %CC% %COMMON_FLAGS% %INCLUDES% -c "%%f" -o "!OBJ_FILE!"
        if !errorlevel! neq 0 goto :error
    ) else (
        echo   %%~nxf [Skipped - up to date]
    )
)

echo Compiling main files...
for %%f in ("%SRC_DIR%\*.c") do (
    set "SOURCE_FILE=%%f"
    set "OBJ_FILE=%BUILD_DIR%\%%~nf.o"
    
    REM Check if object file exists and source is newer
    set "NEED_COMPILE=0"
    if not exist "!OBJ_FILE!" set NEED_COMPILE=1
    if %FORCE_REBUILD%==1 set NEED_COMPILE=1
    if !NEED_COMPILE!==0 (
        for /f "tokens=*" %%t in ('forfiles /p "%SRC_DIR%" /m "%%~nxf" /c "cmd /c echo @fdate @ftime"') do set SRC_TIME=%%t
        if exist "!OBJ_FILE!" (
            for /f "tokens=*" %%t in ('forfiles /p "%BUILD_DIR%" /m "%%~nf.o" /c "cmd /c echo @fdate @ftime"') do set OBJ_TIME=%%t
            if "!SRC_TIME!" gtr "!OBJ_TIME!" set NEED_COMPILE=1
        ) else (
            set NEED_COMPILE=1
        )
    )
    
    if !NEED_COMPILE!==1 (
        echo   %%~nxf
        %CC% %COMMON_FLAGS% %INCLUDES% -c "%%f" -o "!OBJ_FILE!"
        if !errorlevel! neq 0 goto :error
    ) else (
        echo   %%~nxf [Skipped - up to date]
    )
)

REM ==================== Create Libraries ====================
echo Creating libraries...

echo Creating static library...
%AR% rcs "%BUILD_DIR%\libppdb.a" "%BUILD_DIR%\*.o"
if !errorlevel! neq 0 goto :error

echo Creating executable...
%CC% %COMMON_FLAGS% %LDFLAGS% -o "%BUILD_DIR%\ppdb.exe.dbg" "%BUILD_DIR%\*.o" "%COSMO_DIR%\crt.o" "%COSMO_DIR%\ape-no-modify-self.o" "%COSMO_DIR%\cosmopolitan.a"
if !errorlevel! neq 0 goto :error

echo Creating final binary...
%OBJCOPY% -S -O binary "%BUILD_DIR%\ppdb.exe.dbg" "%BUILD_DIR%\ppdb.exe"
if !errorlevel! neq 0 goto :error

REM ==================== Copy Header Files ====================
echo Copying header files...
copy "%ROOT_DIR%\include\ppdb\*.h" "%BUILD_DIR%\include\ppdb\"
if !errorlevel! neq 0 goto :error

echo Build completed successfully
goto :eof

:error
echo.
echo Build failed with error code !errorlevel!
echo Current directory: %CD%
echo Build directory: %BUILD_DIR%
exit /b !errorlevel!
