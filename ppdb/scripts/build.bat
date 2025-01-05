@echo off
rem ===== Note: This batch file uses English only to avoid Windows encoding issues =====
chcp 65001 > nul
setlocal EnableDelayedExpansion

rem ===== Get Command Line Parameters =====
set "TEST_TYPE=%1"
set "BUILD_MODE=%2"
if "%TEST_TYPE%"=="" set "TEST_TYPE=help"
if "%BUILD_MODE%"=="" set "BUILD_MODE=release"

rem ===== Validate Build Mode =====
if /i not "%BUILD_MODE%"=="debug" if /i not "%BUILD_MODE%"=="release" (
    echo Invalid build mode: %BUILD_MODE%
    echo Valid modes are: debug, release
    exit /b 1
)

rem ===== Process Commands =====

rem ===== Test42 =====
if /i "%TEST_TYPE%"=="test42" (
    call "%~dp0\build_test42.bat" %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

rem ===== Clean Command =====
if /i "%TEST_TYPE%"=="clean" (
    call "%~dp0\build_clean.bat"
    exit /b %ERRORLEVEL%
)

rem ===== Force Rebuild =====
if /i "%TEST_TYPE%"=="rebuild" (
    call "%~dp0\build_clean.bat"
    if errorlevel 1 exit /b 1
    call "%~dp0\build.bat" test42 %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

rem ===== Sync Tests =====
if /i "%TEST_TYPE%"=="sync_locked" (
    call "%~dp0\build_sync.bat" locked %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

if /i "%TEST_TYPE%"=="sync_lockfree" (
    call "%~dp0\build_sync.bat" lockfree %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

rem ===== Skiplist Tests =====
if /i "%TEST_TYPE%"=="skiplist_locked" (
    call "%~dp0\build_skiplist.bat" locked %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

if /i "%TEST_TYPE%"=="skiplist_lockfree" (
    call "%~dp0\build_skiplist.bat" lockfree %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

rem ===== Memtable Tests =====
if /i "%TEST_TYPE%"=="memtable_locked" (
    call "%~dp0\build_memtable.bat" locked %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

if /i "%TEST_TYPE%"=="memtable_lockfree" (
    call "%~dp0\build_memtable.bat" lockfree %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

rem ===== Sharded Memtable Tests =====
if /i "%TEST_TYPE%"=="sharded_locked" (
    call "%~dp0\build_sharded.bat" locked %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

if /i "%TEST_TYPE%"=="sharded_lockfree" (
    call "%~dp0\build_sharded.bat" lockfree %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

rem ===== KVStore Tests =====
if /i "%TEST_TYPE%"=="kvstore" (
    call "%~dp0\build_kvstore.bat" %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

rem ===== WAL Tests =====
if /i "%TEST_TYPE%"=="wal_core" (
    call "%~dp0\build_wal.bat" core %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

if /i "%TEST_TYPE%"=="wal_func" (
    call "%~dp0\build_wal.bat" func %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

if /i "%TEST_TYPE%"=="wal_advanced" (
    call "%~dp0\build_wal.bat" advanced %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

rem ===== Async Tests =====
if /i "%TEST_TYPE%"=="async_core" (
    call "%~dp0\build_async.bat" core %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

if /i "%TEST_TYPE%"=="async_timer" (
    call "%~dp0\build_async.bat" timer %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

if /i "%TEST_TYPE%"=="async_future" (
    call "%~dp0\build_async.bat" future %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

if /i "%TEST_TYPE%"=="async_iocp" (
    call "%~dp0\build_async.bat" iocp %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

rem ===== Performance Tests =====
if /i "%TEST_TYPE%"=="perf" (
    call "%~dp0\build_perf.bat" %BUILD_MODE%
    exit /b %ERRORLEVEL%
)

rem ===== Display Help Information =====
if /i "%TEST_TYPE%"=="help" (
    echo PPDB Build and Test Tool
    echo.
    echo Usage: build.bat [module] [build mode]
    echo.
    echo Available modules:
    echo   test42            Run basic tests
    echo   sync_locked       Run locked synchronization tests
    echo   sync_lockfree     Run lock-free synchronization tests
    echo   skiplist_locked   Run locked skiplist tests
    echo   skiplist_lockfree Run lock-free skiplist tests
    echo   memtable_locked   Run locked memtable tests
    echo   memtable_lockfree Run lock-free memtable tests
    echo   sharded_locked    Run sharded memtable tests
    echo   sharded_lockfree  Run sharded memtable tests
    echo.
    echo   kvstore          Run KVStore tests
    echo   wal_core         Run WAL core tests
    echo   wal_func         Run WAL function tests
    echo   wal_advanced     Run WAL advanced tests
    echo   async_core       Run async core tests
    echo   async_timer      Run async timer tests
    echo   async_future     Run async future tests
    echo   async_iocp       Run async IOCP tests
    echo   perf            Run performance tests
    echo   clean            Clean build directory
    echo   rebuild          Force rebuild
    echo.
    echo Build modes:
    echo   debug     Debug mode
    echo   release   Release mode (default)
    echo.
    echo Examples:
    echo   build.bat help                Display help information
    echo   build.bat clean               Clean build directory
    echo   build.bat rebuild             Force rebuild
    echo   build.bat test42              Run basic tests
    echo   build.bat sync_locked debug   Run locked synchronization tests in debug mode
    echo   build.bat sync_lockfree       Run lock-free synchronization tests in release mode
    exit /b 0
)

rem ===== Invalid Command =====
echo Invalid test type: %TEST_TYPE%
echo Run 'build.bat help' for usage information
exit /b 1 
