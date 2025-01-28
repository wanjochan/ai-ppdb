@echo off
setlocal enabledelayedexpansion

set MODULE=%1
if "%MODULE%"=="" (
    echo Usage: build_test_poly.bat ^<module^>
    echo Available modules:
    echo   cmdline    Command line framework test
    echo   rinetd     RINETD module test
    exit /b 1
)

set NORUN=%2

set ROOT_DIR=%~dp0..
set BUILD_DIR=%ROOT_DIR%\build\test\poly
set SRC_DIR=%ROOT_DIR%\src
set TEST_DIR=%ROOT_DIR%\test\white

if not exist %BUILD_DIR% mkdir %BUILD_DIR%

set CFLAGS=-I%SRC_DIR%\internal -I%TEST_DIR% -g -O0

if "%MODULE%"=="cmdline" (
    echo Building command line framework test...
    gcc %CFLAGS% ^
        %SRC_DIR%\internal\poly\poly_cmdline.c ^
        %TEST_DIR%\poly\test_cmdline.c ^
        %TEST_DIR%\poly\test_cmdline_main.c ^
        -o %BUILD_DIR%\test_cmdline.exe

    if "%NORUN%"=="norun" (
        echo Test built successfully
        exit /b 0
    )

    echo Running command line framework test...
    %BUILD_DIR%\test_cmdline.exe
) else if "%MODULE%"=="rinetd" (
    echo Building RINETD module test...
    gcc %CFLAGS% ^
        %SRC_DIR%\internal\peer\peer_rinetd.c ^
        %TEST_DIR%\peer\test_peer_rinetd.c ^
        %TEST_DIR%\peer\test_peer_rinetd_main.c ^
        -o %BUILD_DIR%\test_rinetd.exe

    if "%NORUN%"=="norun" (
        echo Test built successfully
        exit /b 0
    )

    echo Running RINETD module test...
    %BUILD_DIR%\test_rinetd.exe
) else (
    echo Unknown module: %MODULE%
    exit /b 1
) 