@echo off
setlocal EnableDelayedExpansion

rem Get build mode and test mode from parameters
set "TEST_MODE=%1"
set "BUILD_MODE=%2"
if "%BUILD_MODE%"=="" set "BUILD_MODE=release"

rem Load environment variables and common functions
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

rem Create build directory if it doesn't exist
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem Build base library
echo Building base library...
%GCC% %CFLAGS% -c "%PPDB_DIR%\src\base.c" -o "%BUILD_DIR%\base.o"
if errorlevel 1 exit /b 1

if not "%TEST_MODE%"=="notest" (
    echo Running base tests...
    REM Build and run memory test
    echo Building memory test...
    %GCC% %CFLAGS% "%BUILD_DIR%\base.o" "%PPDB_DIR%\test\white\infra\test_memory.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\memory_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\memory_test.exe.dbg" "%BUILD_DIR%\memory_test.exe"
    if errorlevel 1 exit /b 1

    echo Running memory test...
    "%BUILD_DIR%\memory_test.exe"
    if errorlevel 1 exit /b 1

    REM Build and run error test
    echo Building error test...
    %GCC% %CFLAGS% "%BUILD_DIR%\base.o" "%PPDB_DIR%\test\white\infra\test_error.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\error_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\error_test.exe.dbg" "%BUILD_DIR%\error_test.exe"
    if errorlevel 1 exit /b 1

    echo Running error test...
    "%BUILD_DIR%\error_test.exe"
    if errorlevel 1 exit /b 1

    REM Build and run log test
    echo Building log test...
    %GCC% %CFLAGS% "%BUILD_DIR%\base.o" "%PPDB_DIR%\test\white\infra\test_log.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\log_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\log_test.exe.dbg" "%BUILD_DIR%\log_test.exe"
    if errorlevel 1 exit /b 1

    echo Running log test...
    "%BUILD_DIR%\log_test.exe"
    if errorlevel 1 exit /b 1

    REM Build and run sync test
    echo Building sync test...
    %GCC% %CFLAGS% "%BUILD_DIR%\base.o" "%PPDB_DIR%\test\white\infra\test_sync.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\sync_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\sync_test.exe.dbg" "%BUILD_DIR%\sync_test.exe"
    if errorlevel 1 exit /b 1

    echo Running sync test...
    "%BUILD_DIR%\sync_test.exe"
    if errorlevel 1 exit /b 1

    REM Build and run skiplist test
    echo Building skiplist test...
    %GCC% %CFLAGS% "%BUILD_DIR%\base.o" "%PPDB_DIR%\test\white\infra\test_skiplist.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\skiplist_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\skiplist_test.exe.dbg" "%BUILD_DIR%\skiplist_test.exe"
    if errorlevel 1 exit /b 1

    echo Running skiplist test...
    "%BUILD_DIR%\skiplist_test.exe"
    if errorlevel 1 exit /b 1

    REM Build and run async test
    echo Building async test...
    %GCC% %CFLAGS% "%BUILD_DIR%\base.o" "%PPDB_DIR%\test\white\base\test_async.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\async_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\async_test.exe.dbg" "%BUILD_DIR%\async_test.exe"
    if errorlevel 1 exit /b 1

    echo Running async test...
    "%BUILD_DIR%\async_test.exe"
    if errorlevel 1 exit /b 1

    REM Build and run timer test
    echo Building timer test...
    %GCC% %CFLAGS% "%BUILD_DIR%\base.o" "%PPDB_DIR%\test\white\base\test_timer.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\timer_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\timer_test.exe.dbg" "%BUILD_DIR%\timer_test.exe"
    if errorlevel 1 exit /b 1

    echo Running timer test...
    "%BUILD_DIR%\timer_test.exe"
    if errorlevel 1 exit /b 1

    REM Build and run event test
    echo Building event test...
    %GCC% %CFLAGS% "%BUILD_DIR%\base.o" "%PPDB_DIR%\test\white\base\test_event.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\event_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\event_test.exe.dbg" "%BUILD_DIR%\event_test.exe"
    if errorlevel 1 exit /b 1

    echo Running event test...
    "%BUILD_DIR%\event_test.exe"
    if errorlevel 1 exit /b 1

    echo All base tests passed!
)

exit /b 0