@echo off
setlocal EnableDelayedExpansion

rem Get build mode from parameter
set "BUILD_MODE=%1"
if "%BUILD_MODE%"=="" set "BUILD_MODE=release"

rem Load environment variables and common functions
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

rem Build performance test
echo Building performance test...
"%GCC%" %CFLAGS% "%PPDB_DIR%\test\core\test_perf.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\perf_test.exe.dbg"
if errorlevel 1 exit /b 1

"%OBJCOPY%" -S -O binary "%BUILD_DIR%\perf_test.exe.dbg" "%BUILD_DIR%\perf_test.exe"
if errorlevel 1 exit /b 1

rem Run the test if not explicitly disabled
if not "%2"=="norun" (
    "%BUILD_DIR%\perf_test.exe"
)

exit /b 0
