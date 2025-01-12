@echo off
setlocal EnableDelayedExpansion

rem Load environment variables and common functions
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

rem Create build directory if it doesn't exist
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem Add source directory to include path
set CFLAGS=%CFLAGS% -I"%SRC_DIR%"

rem Build ppdb
echo Building ppdb...
"%GCC%" %CFLAGS% ^
    "%SRC_DIR%\ppdb\ppdb.c" ^
    "%SRC_DIR%\internal\poly\poly_cmdline.c" ^
    "%SRC_DIR%\internal\peer\peer_rinetd.c" ^
    "%SRC_DIR%\internal\infra\infra_core.c" ^
    "%SRC_DIR%\internal\infra\infra_memory.c" ^
    "%SRC_DIR%\internal\infra\infra_string.c" ^
    "%SRC_DIR%\internal\infra\infra_error.c" ^
    "%SRC_DIR%\internal\infra\infra_mux.c" ^
    "%SRC_DIR%\internal\infra\infra_net.c" ^
    %LDFLAGS% %LIBS% -o "%BUILD_DIR%\ppdb.exe.dbg"
if errorlevel 1 exit /b 1

"%OBJCOPY%" -S -O binary "%BUILD_DIR%\ppdb.exe.dbg" "%BUILD_DIR%\ppdb.exe"
if errorlevel 1 exit /b 1

rem Run the program if not explicitly disabled
if not "%1"=="norun" (
    "%BUILD_DIR%\ppdb.exe" help
)

exit /b 0 