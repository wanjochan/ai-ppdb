@echo off
setlocal EnableDelayedExpansion

rem Load environment variables
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

echo Building database layer...

rem Build database library
%CC% %CFLAGS% -c %SRC_DIR%\database.c -o %BUILD_DIR%\database.o
if errorlevel 1 exit /b 1

rem Run tests if not disabled
if "%1"=="notest" goto :skip_tests

echo Running database tests...
%CC% %CFLAGS% %TEST_DIR%\white\database\test_database.c -o %BUILD_DIR%\test_database.exe %BUILD_DIR%\database.o %BUILD_DIR%\base.o
if errorlevel 1 exit /b 1

%BUILD_DIR%\test_database.exe
if errorlevel 1 exit /b 1

:skip_tests
echo Database layer build completed 