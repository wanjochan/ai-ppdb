@echo off
setlocal EnableDelayedExpansion

rem Load environment variables
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

echo Building ppdb...

rem Build main executable
%CC% %CFLAGS% -c ^
%SRC_DIR%\ppdb.c ^
-o %BUILD_DIR%\ppdb.o
if errorlevel 1 exit /b 1

rem Link executable
%CC% %CFLAGS% ^
%BUILD_DIR%\ppdb.o ^
%BUILD_DIR%\base.o ^
%BUILD_DIR%\database.o ^
%BUILD_DIR%\peer.o ^
%LDFLAGS% %LIBS% ^
-o %BUILD_DIR%\ppdb.exe
if errorlevel 1 exit /b 1

echo PPDB build completed 