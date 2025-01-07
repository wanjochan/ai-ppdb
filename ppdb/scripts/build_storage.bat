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

    REM Build each test file separately
    for %%f in (
        test_storage_init.c
        test_storage_data.c
        test_storage_table.c
        test_storage_maintain.c
        test_memtable.c
        test_sharded_memtable.c
    ) do (
        echo Building %%f...
        %GCC% %CFLAGS% "%BUILD_DIR%\storage.o" "%BUILD_DIR%\base.o" "%PPDB_DIR%\test\white\test_framework.c" "%PPDB_DIR%\test\white\storage\%%f" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\%%~nf.exe.dbg"
        if errorlevel 1 exit /b 1
        "%OBJCOPY%" -S -O binary "%BUILD_DIR%\%%~nf.exe.dbg" "%BUILD_DIR%\%%~nf.exe"
        if errorlevel 1 exit /b 1
        
        echo Running %%f...
        "%BUILD_DIR%\%%~nf.exe"
        if errorlevel 1 exit /b 1
    )

    echo All storage tests passed!
)

exit /b 0