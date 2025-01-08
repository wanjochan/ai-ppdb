@echo off
setlocal EnableDelayedExpansion

rem Get build mode from parameter
set "BUILD_MODE=%1"
if "%BUILD_MODE%"=="" set "BUILD_MODE=release"

rem Load environment variables and common functions
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

rem Create build directory if it doesn't exist
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem Build async_perf
echo Building async_perf...
"%GCC%" %CFLAGS% "%PPDB_DIR%\test\white\base\test_async_perf.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\async_perf.exe.dbg"
if errorlevel 1 exit /b 1

"%OBJCOPY%" -S -O binary "%BUILD_DIR%\async_perf.exe.dbg" "%BUILD_DIR%\async_perf.exe"
if errorlevel 1 exit /b 1

rem Run the test if not explicitly disabled
if not "%2"=="norun" (
    "%BUILD_DIR%\async_perf.exe"
)

exit /b 0 