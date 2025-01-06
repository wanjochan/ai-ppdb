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

rem Build storage library
echo Building storage library...
%GCC% %CFLAGS% -c "%PPDB_DIR%\src\storage.c" -o "%BUILD_DIR%\storage.o"
if errorlevel 1 exit /b 1

if not "%TEST_MODE%"=="notest" (
    echo Running storage tests...
    REM Build and run storage tests
    echo Building storage tests...
    %GCC% %CFLAGS% "%BUILD_DIR%\storage.o" "%BUILD_DIR%\base.o" "%PPDB_DIR%\test\white\storage\test_storage.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\storage_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\storage_test.exe.dbg" "%BUILD_DIR%\storage_test.exe"
    if errorlevel 1 exit /b 1

    echo Running storage tests...
    "%BUILD_DIR%\storage_test.exe"
    if errorlevel 1 exit /b 1

    echo All storage tests passed!
)

exit /b 0