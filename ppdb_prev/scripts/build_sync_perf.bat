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

rem Build sync performance test
echo Building sync performance test...
"%GCC%" %CFLAGS% "%SRC_DIR%\base.c" "%TEST_DIR%\white\base\test_sync_perf.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\test_sync_perf.exe.dbg"
if errorlevel 1 exit /b 1

"%OBJCOPY%" -S -O binary "%BUILD_DIR%\test_sync_perf.exe.dbg" "%BUILD_DIR%\test_sync_perf.exe"
if errorlevel 1 exit /b 1

rem Run the test if not explicitly disabled
if not "%2"=="norun" (
    echo Running sync performance test...
    "%BUILD_DIR%\test_sync_perf.exe"
)

exit /b 0 