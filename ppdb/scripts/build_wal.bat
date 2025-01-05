@echo off
setlocal EnableDelayedExpansion

rem Get test type and build mode from parameters
set "WAL_TEST=%1"
set "BUILD_MODE=%2"
if "%WAL_TEST%"=="" set "WAL_TEST=engine"
if "%BUILD_MODE%"=="" set "BUILD_MODE=release"

rem Validate WAL test type
if /i not "%WAL_TEST%"=="engine" if /i not "%WAL_TEST%"=="func" if /i not "%WAL_TEST%"=="advanced" (
    echo Invalid WAL test type: %WAL_TEST%
    echo Valid types are: engine, func, advanced
    exit /b 1
)

rem Load environment variables and common functions
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

rem Build WAL test
echo Building WAL %WAL_TEST% test...
"%GCC%" %CFLAGS% ^
    "%PPDB_DIR%\src\base.c" ^
    "%PPDB_DIR%\src\storage.c" ^
    "%PPDB_DIR%\test\white\test_framework.c" ^
    "%PPDB_DIR%\test\white\wal\test_wal_%WAL_TEST%.c" ^
    %LDFLAGS% %LIBS% -o "%BUILD_DIR%\wal_%WAL_TEST%_test.exe.dbg"
if errorlevel 1 exit /b 1

"%OBJCOPY%" -S -O binary "%BUILD_DIR%\wal_%WAL_TEST%_test.exe.dbg" "%BUILD_DIR%\wal_%WAL_TEST%_test.exe"
if errorlevel 1 exit /b 1

rem Run the test if not explicitly disabled
if not "%3"=="norun" (
    "%BUILD_DIR%\wal_%WAL_TEST%_test.exe"
)

exit /b 0 