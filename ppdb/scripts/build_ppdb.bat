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

rem Build libppdb first
echo Building libppdb...
"%GCC%" %CFLAGS% -c "%SRC_DIR%\libppdb.c" -o "%BUILD_DIR%\libppdb.o"
if errorlevel 1 exit /b 1

rem Build ppdb
echo Building ppdb...
"%GCC%" %CFLAGS% "%SRC_DIR%\ppdb.c" "%BUILD_DIR%\libppdb.o" "%BUILD_DIR%\base.o" "%BUILD_DIR%\engine.o" "%BUILD_DIR%\storage.o" "%BUILD_DIR%\peer.o" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\ppdb.exe.dbg"
if errorlevel 1 exit /b 1

"%OBJCOPY%" -S -O binary "%BUILD_DIR%\ppdb.exe.dbg" "%BUILD_DIR%\ppdb.exe"
if errorlevel 1 exit /b 1

exit /b 0 