@echo off
setlocal EnableDelayedExpansion

rem Get sync mode and build mode from parameters
set "SYNC_MODE=%1"
set "BUILD_MODE=%2"
if "%SYNC_MODE%"=="" set "SYNC_MODE=locked"
if "%BUILD_MODE%"=="" set "BUILD_MODE=release"

rem Validate sync mode
if /i not "%SYNC_MODE%"=="locked" if /i not "%SYNC_MODE%"=="lockfree" (
    echo Invalid sync mode: %SYNC_MODE%
    echo Valid modes are: locked, lockfree
    exit /b 1
)

rem Set PPDB sync mode
set "PPDB_SYNC_MODE=%SYNC_MODE%"

rem Load environment variables and common functions
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

rem Build sharded memtable test
echo Building sharded memtable %SYNC_MODE% test...
"%GCC%" %CFLAGS% ^
    "%PPDB_DIR%\src\base.c" ^
    "%PPDB_DIR%\src\storage.c" ^
    "%PPDB_DIR%\test\white\test_framework.c" ^
    "%PPDB_DIR%\test\white\storage\test_sharded_memtable.c" ^
    %LDFLAGS% %LIBS% -o "%BUILD_DIR%\sharded_%SYNC_MODE%_test.exe.dbg"
if errorlevel 1 exit /b 1

"%OBJCOPY%" -S -O binary "%BUILD_DIR%\sharded_%SYNC_MODE%_test.exe.dbg" "%BUILD_DIR%\sharded_%SYNC_MODE%_test.exe"
if errorlevel 1 exit /b 1

rem Run the test if not explicitly disabled
if not "%3"=="norun" (
    "%BUILD_DIR%\sharded_%SYNC_MODE%_test.exe"
)

exit /b 0 