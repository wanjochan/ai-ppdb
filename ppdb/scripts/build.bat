@echo off
rem ===== Note: This batch file uses English only to avoid Windows encoding issues =====
chcp 65001 > nul
setlocal EnableDelayedExpansion

rem ===== Get Command Line Parameters =====
set "TEST_TYPE=%1"
set "BUILD_MODE=%2"
if "%TEST_TYPE%"=="" set "TEST_TYPE=help"
if "%BUILD_MODE%"=="" set "BUILD_MODE=release"

rem ===== Validate Build Mode =====
if /i not "%BUILD_MODE%"=="debug" if /i not "%BUILD_MODE%"=="release" (
    echo Invalid build mode: %BUILD_MODE%
    echo Valid modes are: debug, release
    exit /b 1
)

rem ===== Process Commands =====

rem ===== Test42 =====
if /i "%TEST_TYPE%"=="test42" (
    call "%~dp0\build_test42.bat" %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

rem ===== Clean Command =====
if /i "%TEST_TYPE%"=="clean" (
    call "%~dp0\build_clean.bat"
    exit /b %ERRORLEVEL%
)

rem ===== Force Rebuild =====
if /i "%TEST_TYPE%"=="rebuild" (
    call "%~dp0\build_clean.bat"
    if errorlevel 1 exit /b 1
    call "%~dp0\build.bat" test42 %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

rem ===== Base Layer Tests =====
if /i "%TEST_TYPE%"=="base" (
    call "%~dp0\build_base.bat" %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

rem ===== Core Layer Tests =====
if /i "%TEST_TYPE%"=="core" (
    call "%~dp0\build_core.bat" %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

rem ===== Sync Tests =====
if /i "%TEST_TYPE%"=="sync_locked" (
    call "%~dp0\build_sync.bat" locked %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

if /i "%TEST_TYPE%"=="sync_lockfree" (
    call "%~dp0\build_sync.bat" lockfree %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

rem ===== Skiplist Test =====
if /i "%TEST_TYPE%"=="skiplist" (
    call "%~dp0\build_skiplist.bat" %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

rem ===== Help =====
if /i "%TEST_TYPE%"=="help" (
    echo Usage: build.bat [test_type] [build_mode]
    echo.
    echo Test Types:
    echo   test42       - Run basic tests
    echo   base         - Run all base layer tests
    echo   core         - Run all core layer tests
    echo   sync_locked  - Run sync tests with locks
    echo   sync_lockfree- Run sync tests without locks
    echo   skiplist     - Run skiplist tests
    echo   clean        - Clean build files
    echo   rebuild      - Clean and rebuild
    echo   help         - Show this help
    echo.
    echo Build Modes:
    echo   debug        - Debug build
    echo   release      - Release build ^(default^)
    exit /b 0
)

echo Invalid test type: %TEST_TYPE%
echo Run 'build.bat help' for usage information
exit /b 1 
