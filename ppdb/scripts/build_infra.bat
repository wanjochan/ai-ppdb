@echo off
setlocal

rem Build environment setup
call "%~dp0build_env.bat"
if errorlevel 1 exit /b 1

rem Create build directories if they don't exist
if not exist "%BUILD_DIR%\infra" mkdir "%BUILD_DIR%\infra"

echo Building infra layer...

rem Build core module
"%GCC%" %CFLAGS% -I"%SRC_DIR%" "%PPDB_DIR%\src\internal\infra\infra_core.c" -c -o "%BUILD_DIR%\infra\infra_core.o"
if errorlevel 1 exit /b 1

rem Build platform module
"%GCC%" %CFLAGS% -I"%SRC_DIR%" "%PPDB_DIR%\src\internal\infra\infra_platform.c" -c -o "%BUILD_DIR%\infra\infra_platform.o"
if errorlevel 1 exit /b 1

rem Build sync module
"%GCC%" %CFLAGS% -I"%SRC_DIR%" "%PPDB_DIR%\src\internal\infra\infra_sync.c" -c -o "%BUILD_DIR%\infra\infra_sync.o"
if errorlevel 1 exit /b 1

rem Build error module
"%GCC%" %CFLAGS% -I"%SRC_DIR%" "%PPDB_DIR%\src\internal\infra\infra_error.c" -c -o "%BUILD_DIR%\infra\infra_error.o"
if errorlevel 1 exit /b 1

rem Build struct module
"%GCC%" %CFLAGS% -I"%SRC_DIR%" "%PPDB_DIR%\src\internal\infra\infra_struct.c" -c -o "%BUILD_DIR%\infra\infra_struct.o"
if errorlevel 1 exit /b 1

rem Create static library
"%AR%" rcs "%BUILD_DIR%\infra\libinfra.a" "%BUILD_DIR%\infra\infra_core.o" "%BUILD_DIR%\infra\infra_platform.o" "%BUILD_DIR%\infra\infra_sync.o" "%BUILD_DIR%\infra\infra_error.o" "%BUILD_DIR%\infra\infra_struct.o"
if errorlevel 1 exit /b 1

echo Build infra complete.
exit /b 0 