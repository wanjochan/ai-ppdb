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
if not exist "%BUILD_DIR%\infra" mkdir "%BUILD_DIR%\infra"
if not exist "%BUILD_DIR%\test" mkdir "%BUILD_DIR%\test"

rem Build infra layer
echo Building infra layer...
"%GCC%" %CFLAGS% "%PPDB_DIR%\src\infra\infra_core.c" -c -o "%BUILD_DIR%\infra\infra_core.o"
if errorlevel 1 exit /b 1

"%GCC%" %CFLAGS% "%PPDB_DIR%\src\infra\infra_async.c" -c -o "%BUILD_DIR%\infra\infra_async.o"
if errorlevel 1 exit /b 1

"%GCC%" %CFLAGS% "%PPDB_DIR%\src\infra\infra_event.c" -c -o "%BUILD_DIR%\infra\infra_event.o"
if errorlevel 1 exit /b 1

"%GCC%" %CFLAGS% "%PPDB_DIR%\src\infra\infra_io.c" -c -o "%BUILD_DIR%\infra\infra_io.o"
if errorlevel 1 exit /b 1

"%GCC%" %CFLAGS% "%PPDB_DIR%\src\infra\infra_peer.c" -c -o "%BUILD_DIR%\infra\infra_peer.o"
if errorlevel 1 exit /b 1

rem Create static library
"%AR%" rcs "%BUILD_DIR%\infra\libinfra.a" ^
    "%BUILD_DIR%\infra\infra_core.o" ^
    "%BUILD_DIR%\infra\infra_async.o" ^
    "%BUILD_DIR%\infra\infra_event.o" ^
    "%BUILD_DIR%\infra\infra_io.o" ^
    "%BUILD_DIR%\infra\infra_peer.o"
if errorlevel 1 exit /b 1

rem Build test
echo Building infra test...
"%GCC%" %CFLAGS% "%PPDB_DIR%\test\white\infra\test_infra.c" -o "%BUILD_DIR%\test\test_infra.exe.dbg" ^
    -L"%BUILD_DIR%\infra" -linfra %LDFLAGS% %LIBS%
if errorlevel 1 exit /b 1

"%OBJCOPY%" -S -O binary "%BUILD_DIR%\test\test_infra.exe.dbg" "%BUILD_DIR%\test\test_infra.exe"
if errorlevel 1 exit /b 1

rem Run the test if not explicitly disabled
if not "%2"=="norun" (
    "%BUILD_DIR%\test\test_infra.exe"
)

exit /b 0 