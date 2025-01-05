@echo off
setlocal EnableDelayedExpansion

rem Get target and build mode from parameters
set "TARGET=%1"
set "BUILD_MODE=%2"
if "%BUILD_MODE%"=="" set "BUILD_MODE=release"

rem Load environment variables and common functions
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

rem Create build directory if it doesn't exist
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem Build target
if "%TARGET%"=="test42" (
    call "%~dp0\build_test42.bat" %BUILD_MODE%
    exit /b !errorlevel!
) else if "%TARGET%"=="base" (
    call "%~dp0\build_base.bat" %BUILD_MODE%
    exit /b !errorlevel!
) else if "%TARGET%"=="core" (
    call "%~dp0\build_core.bat" %BUILD_MODE%
    exit /b !errorlevel!
) else if "%TARGET%"=="store" (
    call "%~dp0\build_store.bat" %BUILD_MODE%
    exit /b !errorlevel!
) else if "%TARGET%"=="all" (
    call "%~dp0\build_base.bat" %BUILD_MODE%
    if errorlevel 1 exit /b 1
    call "%~dp0\build_core.bat" %BUILD_MODE%
    if errorlevel 1 exit /b 1
    call "%~dp0\build_store.bat" %BUILD_MODE%
    exit /b !errorlevel!
) else (
    echo Error: Unknown target %TARGET%
    echo Available targets: test42, base, core, store, all
    exit /b 1
) 
