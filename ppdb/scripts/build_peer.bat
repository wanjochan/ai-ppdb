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

rem Build dependencies
call "%~dp0\build_base.bat"
if errorlevel 1 exit /b 1

call "%~dp0\build_storage.bat"
if errorlevel 1 exit /b 1

rem Build libppdb first
echo Building libppdb...
%GCC% %CFLAGS% -c "%PPDB_DIR%\src\libppdb.c" -o "%BUILD_DIR%\libppdb.o"
if errorlevel 1 exit /b 1

rem Build peer library
echo Building peer library...
%GCC% %CFLAGS% -I"%PPDB_DIR%\src\peer" -c "%PPDB_DIR%\src\peer.c" -o "%BUILD_DIR%\peer.o"
if errorlevel 1 exit /b 1

if not "%TEST_MODE%"=="notest" (
    echo Running peer tests...
    REM Build test framework
    echo Building test framework...
    %GCC% %CFLAGS% -c "%PPDB_DIR%\test\white\test_framework.c" -o "%BUILD_DIR%\test_framework.o"
    if errorlevel 1 exit /b 1

    REM Build and run peer tests
    echo Building peer tests...

    REM Build each test file separately
    for %%f in (
        test_peer_init.c
        test_peer_conn.c
        test_peer_proto.c
        test_peer_memcached.c
        test_peer_server.c
    ) do (
        echo Building %%f...
        %GCC% %CFLAGS% -I"%PPDB_DIR%\src\peer" "%BUILD_DIR%\peer.o" "%BUILD_DIR%\base.o" "%BUILD_DIR%\storage.o" "%BUILD_DIR%\libppdb.o" "%BUILD_DIR%\test_framework.o" "%PPDB_DIR%\test\white\peer\%%f" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\%%~nf.exe.dbg"
        if errorlevel 1 exit /b 1
        "%OBJCOPY%" -S -O binary "%BUILD_DIR%\%%~nf.exe.dbg" "%BUILD_DIR%\%%~nf.exe"
        if errorlevel 1 exit /b 1
        
        echo Running %%f...
        "%BUILD_DIR%\%%~nf.exe"
        if errorlevel 1 exit /b 1
    )

    echo All peer tests passed!
)

exit /b 0 