@echo off
setlocal EnableDelayedExpansion

rem Get build mode and test mode from parameters
set "BUILD_MODE=%1"
if "%BUILD_MODE%"=="" set "BUILD_MODE=release"

rem Load environment variables and common functions
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

rem Create build directory if it doesn't exist
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem Build base library if not exists
if not exist "%BUILD_DIR%\base.o" (
    echo Building base library...
    call "%~dp0\build_base.bat" %BUILD_MODE%
    if errorlevel 1 exit /b 1
)

rem Build engine library
echo Building engine library...
%GCC% %CFLAGS% -c "%SRC_DIR%\engine.c" -o "%BUILD_DIR%\engine.o"
if errorlevel 1 exit /b 1

if not "%BUILD_MODE%"=="notest" (
    echo Running engine tests...
    REM Build and run engine tests
    echo Building engine tests...
    %GCC% %CFLAGS% -I"%PPDB_DIR%" -I"%SRC_DIR%" -I"%SRC_DIR%\internal" "%TEST_DIR%\white\engine\test_engine.c" "%BUILD_DIR%\base.o" "%BUILD_DIR%\engine.o" -lpthread %LDFLAGS% %LIBS% -o "%BUILD_DIR%\engine_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\engine_test.exe.dbg" "%BUILD_DIR%\engine_test.exe"
    if errorlevel 1 exit /b 1

    echo Running engine tests...
    "%BUILD_DIR%\engine_test.exe"
    if errorlevel 1 exit /b 1

    echo All engine tests passed!
)

exit /b 0
