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
) else if "%TARGET%"=="sync_perf" (
    call "%~dp0\build_sync_perf.bat" %BUILD_MODE%
    exit /b !errorlevel!
) else if "%TARGET%"=="base" (
    call "%~dp0\build_base.bat" %BUILD_MODE%
    exit /b !errorlevel!
) else if "%TARGET%"=="database" (
    call "%~dp0\build_base.bat" %BUILD_MODE%
    if errorlevel 1 exit /b 1
    call "%~dp0\build_database.bat" %BUILD_MODE%
    exit /b !errorlevel!
) else if "%TARGET%"=="peer" (
    call "%~dp0\build_base.bat" %BUILD_MODE%
    if errorlevel 1 exit /b 1
    call "%~dp0\build_database.bat" %BUILD_MODE%
    if errorlevel 1 exit /b 1
    call "%~dp0\build_peer.bat" %BUILD_MODE%
    exit /b !errorlevel!
) else if "%TARGET%"=="ppdb" (
    echo Building ppdb...
    if not exist "%BUILD_DIR%\base.o" (
        call "%~dp0\build_base.bat" notest %BUILD_MODE%
        if errorlevel 1 exit /b 1
    )
    if not exist "%BUILD_DIR%\database.o" (
        call "%~dp0\build_database.bat" notest %BUILD_MODE%
        if errorlevel 1 exit /b 1
    )
    call "%~dp0\build_ppdb.bat" %BUILD_MODE%
    if errorlevel 1 exit /b 1
    echo Build completed successfully
    exit /b !errorlevel!
) else if "%TARGET%"=="all" (
    echo Building all targets...
    call "%~dp0\build_base.bat" notest %BUILD_MODE%
    if errorlevel 1 exit /b 1
    call "%~dp0\build_database.bat" notest %BUILD_MODE%
    if errorlevel 1 exit /b 1
    call "%~dp0\build_peer.bat" notest %BUILD_MODE%
    if errorlevel 1 exit /b 1
    call "%~dp0\build_ppdb.bat" %BUILD_MODE%
    if errorlevel 1 exit /b 1
    echo Build completed successfully
    exit /b !errorlevel!
) else (
    echo Error: Unknown target %TARGET%
    echo Available targets: test42, sync_perf, base, database, peer, ppdb, all
    exit /b 1
) 
