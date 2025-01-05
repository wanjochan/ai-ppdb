@echo off
setlocal EnableDelayedExpansion

rem ===== Get Build Mode =====
set "BUILD_MODE=%1"
if "%BUILD_MODE%"=="" set "BUILD_MODE=release"

rem ===== Set Compiler Options =====
set "CFLAGS=-Wall -Wextra"
if /i "%BUILD_MODE%"=="debug" (
    set "CFLAGS=!CFLAGS! -g -O0 -DDEBUG"
) else (
    set "CFLAGS=!CFLAGS! -O2 -DNDEBUG"
)

rem ===== Set Paths =====
set "ROOT_DIR=%~dp0.."
set "BUILD_DIR=!ROOT_DIR!\build\skiplist"
set "TEST_DIR=!ROOT_DIR!\test\white\infra"
set "SRC_DIR=!ROOT_DIR!\src"

rem ===== Create Build Directory =====
if not exist "!BUILD_DIR!" mkdir "!BUILD_DIR!"

rem ===== Compile Test =====
echo Building skiplist test...
gcc !CFLAGS! -I"!ROOT_DIR!\include" ^
    "!TEST_DIR!\test_skiplist.c" ^
    "!SRC_DIR!\base.c" ^
    -o "!BUILD_DIR!\test_skiplist.exe"

if errorlevel 1 (
    echo Failed to build skiplist test
    exit /b 1
)

rem ===== Run Test =====
echo Running skiplist test...
"!BUILD_DIR!\test_skiplist.exe"
if errorlevel 1 (
    echo Skiplist test failed
    exit /b 1
)

echo Skiplist test completed successfully
exit /b 0 