@echo off
setlocal EnableDelayedExpansion

rem Get build mode and test type from parameters
set "TEST_TYPE=%1"
set "BUILD_MODE=%2"
if "%BUILD_MODE%"=="" set "BUILD_MODE=release"

rem Load environment variables and common functions
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

rem Set test file based on type
set "TEST_FILE="
if "%TEST_TYPE%"=="core" (
    set "TEST_FILE=test_async.c"
) else if "%TEST_TYPE%"=="timer" (
    set "TEST_FILE=test_timer.c"
) else if "%TEST_TYPE%"=="future" (
    set "TEST_FILE=test_future.c"
) else if "%TEST_TYPE%"=="iocp" (
    set "TEST_FILE=test_iocp.c"
) else (
    echo Invalid test type: %TEST_TYPE%
    echo Valid types are: core, timer, future, iocp
    exit /b 1
)

rem Build test
echo Building async %TEST_TYPE% test...
"%GCC%" %CFLAGS% "%PPDB_DIR%\test\core\%TEST_FILE%" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\async_%TEST_TYPE%_test.exe.dbg"
if errorlevel 1 exit /b 1

"%OBJCOPY%" -S -O binary "%BUILD_DIR%\async_%TEST_TYPE%_test.exe.dbg" "%BUILD_DIR%\async_%TEST_TYPE%_test.exe"
if errorlevel 1 exit /b 1

rem Run the test if not explicitly disabled
if not "%3"=="norun" (
    "%BUILD_DIR%\async_%TEST_TYPE%_test.exe"
)

exit /b 0
