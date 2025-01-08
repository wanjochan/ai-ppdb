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

rem Build storage skiplist test
echo Building storage skiplist test...
%GCC% %CFLAGS% "%BUILD_DIR%\base.o" "%BUILD_DIR%\storage.o" "%PPDB_DIR%\test\white\test_framework.c" "%PPDB_DIR%\test\white\storage\test_storage_skiplist_lockfree.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\test_storage_skiplist_lockfree.exe.dbg"
if errorlevel 1 exit /b 1

"%OBJCOPY%" -S -O binary "%BUILD_DIR%\test_storage_skiplist_lockfree.exe.dbg" "%BUILD_DIR%\test_storage_skiplist_lockfree.exe"
if errorlevel 1 exit /b 1

if not "%TEST_MODE%"=="notest" (
    echo Running storage skiplist test...
    "%BUILD_DIR%\test_storage_skiplist_lockfree.exe"
    if errorlevel 1 (
        echo Storage skiplist test failed with error code !errorlevel!
        exit /b !errorlevel!
    )
    echo Storage skiplist test passed!
)

exit /b 0 