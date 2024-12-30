@echo off
setlocal EnableDelayedExpansion

rem Set paths if not already set
if "%ROOT_DIR%"=="" (
    set "SCRIPT_DIR=%~dp0"
    cd /d "%SCRIPT_DIR%\.."
    set "ROOT_DIR=%CD%"
)
if "%BUILD_DIR%"=="" set "BUILD_DIR=%ROOT_DIR%\build"
if "%COSMO%"=="" set "COSMO=%ROOT_DIR%\..\cosmopolitan"
if "%CROSS9%"=="" set "CROSS9=%ROOT_DIR%\..\cross9\bin"
if "%GCC%"=="" set "GCC=%CROSS9%\x86_64-pc-linux-gnu-gcc.exe"
if "%AR%"=="" set "AR=%CROSS9%\x86_64-pc-linux-gnu-ar.exe"
if "%OBJCOPY%"=="" set "OBJCOPY=%CROSS9%\x86_64-pc-linux-gnu-objcopy.exe"

rem Check directories
if not exist "%COSMO%" (
    echo Error: Cosmopolitan directory not found at %COSMO%
    exit /b 1
)

if not exist "%CROSS9%" (
    echo Error: Cross9 directory not found at %CROSS9%
    exit /b 1
)

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

rem Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem Set compilation flags if not already set
if "%COMMON_FLAGS%"=="" set "COMMON_FLAGS=-Wall -Wextra -fno-pie -fno-stack-protector -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -fno-asynchronous-unwind-tables"
if "%DEBUG_FLAGS%"=="" set "DEBUG_FLAGS=-g -O0 -DDEBUG"

rem Build Cosmopolitan objects
echo Building Cosmopolitan objects...

rem Copy necessary Cosmopolitan files
echo Copying Cosmopolitan files...
copy /Y "%COSMO%\ape.lds" "%BUILD_DIR%\" >nul
copy /Y "%COSMO%\crt.o" "%BUILD_DIR%\" >nul
copy /Y "%COSMO%\ape.o" "%BUILD_DIR%\" >nul
copy /Y "%COSMO%\cosmopolitan.h" "%BUILD_DIR%\" >nul
copy /Y "%COSMO%\cosmopolitan.a" "%BUILD_DIR%\" >nul

if errorlevel 1 (
    echo Error: Failed to copy Cosmopolitan files
    exit /b 1
)

echo Cosmopolitan build completed successfully

rem Export environment variables back to parent script
endlocal & (
    set "ROOT_DIR=%ROOT_DIR%"
    set "BUILD_DIR=%BUILD_DIR%"
    set "COSMO=%COSMO%"
    set "CROSS9=%CROSS9%"
    set "GCC=%GCC%"
    set "AR=%AR%"
    set "OBJCOPY=%OBJCOPY%"
    set "COMMON_FLAGS=%COMMON_FLAGS%"
    set "DEBUG_FLAGS=%DEBUG_FLAGS%"
) 