@echo off
setlocal enabledelayedexpansion

REM Set build environment variables
set BUILD_DIR=..\build
set TEST_DIR=..\test
set BASE_DIR=..\src\base
set ENGINE_DIR=..\src\engine

REM Create build directory if not exists
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

REM Compile base layer tests
echo Building base layer tests...
cl /EHsc /I%BASE_DIR% /I%ENGINE_DIR% ^
   %TEST_DIR%\base\core\*.cpp ^
   %TEST_DIR%\base\util\*.cpp ^
   /Fe%BUILD_DIR%\base_tests.exe

REM Compile engine layer tests
echo Building engine layer tests...
cl /EHsc /I%BASE_DIR% /I%ENGINE_DIR% ^
   %TEST_DIR%\engine\storage\*.cpp ^
   %TEST_DIR%\engine\query\*.cpp ^
   /Fe%BUILD_DIR%\engine_tests.exe

REM Run tests if compilation successful
if %ERRORLEVEL% == 0 (
    echo Running tests...
    %BUILD_DIR%\base_tests.exe
    %BUILD_DIR%\engine_tests.exe
)

echo Build complete.