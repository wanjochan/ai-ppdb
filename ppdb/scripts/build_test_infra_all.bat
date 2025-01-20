@echo off
setlocal enabledelayedexpansion

rem 记录开始时间
set START_TIME=%time%

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%\..\build"

echo Cleaning old build files...
if exist "%BUILD_DIR%\test\white\infra" (
    del /q "%BUILD_DIR%\test\white\infra\*.o" 2>nul
    del /q "%BUILD_DIR%\test\white\infra\*.exe" 2>nul
    del /q "%BUILD_DIR%\test\white\infra\*.dbg" 2>nul
)

echo Building all infra tests...

set MODULES=memory memory_pool error sync log struct net mux
set BUILD_FAILED=0

for %%m in (%MODULES%) do (
    echo.
    echo ========== Building %%m module ==========
    call %SCRIPT_DIR%\build_test_infra.bat %%m
    if errorlevel 1 (
        echo ERROR: Failed to build %%m module
        set BUILD_FAILED=1
        goto :end
    )
)

:end
rem 计算耗时
set END_TIME=%time%
set options="tokens=1-4 delims=:.,"
for /f %options% %%a in ("%START_TIME%") do set start_h=%%a&set /a start_m=100%%b %% 100&set /a start_s=100%%c %% 100&set /a start_ms=100%%d %% 100
for /f %options% %%a in ("%END_TIME%") do set end_h=%%a&set /a end_m=100%%b %% 100&set /a end_s=100%%c %% 100&set /a end_ms=100%%d %% 100

set /a hours=%end_h%-%start_h%
set /a mins=%end_m%-%start_m%
set /a secs=%end_s%-%start_s%
set /a ms=%end_ms%-%start_ms%
if %ms% lss 0 set /a secs = %secs% - 1 & set /a ms = 1000%ms%
if %secs% lss 0 set /a mins = %mins% - 1 & set /a secs = 60%secs%
if %mins% lss 0 set /a hours = %hours% - 1 & set /a mins = 60%mins%
if %hours% lss 0 set /a hours = 24%hours%

echo.
echo ========== Build Summary ==========
if %BUILD_FAILED%==1 (
    echo Build FAILED
    echo Total build time: %hours%:%mins%:%secs%.%ms% (HH:MM:SS.MS)
    exit /b 1
) else (
    echo Build completed successfully
    echo Total build time: %hours%:%mins%:%secs%.%ms% (HH:MM:SS.MS)
    exit /b 0
)
