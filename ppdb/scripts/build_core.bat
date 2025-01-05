@echo off
setlocal

rem Load build environment
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

rem Create output directories if they don't exist
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem Build core test
"%GCC%" %CFLAGS% -I%INCLUDE_DIR% ^
    %SRC_DIR%\core.c ^
    %TEST_DIR%\white\core\test_core.c ^
    -o %BIN_DIR%\test_core.exe ^
    %LDFLAGS% %LIBS%
if errorlevel 1 exit /b 1

rem Run tests
echo Running core tests...
"%BIN_DIR%\test_core.exe"
if errorlevel 1 (
    echo Core tests failed!
    exit /b 1
) else (
    echo Core tests passed.
)

endlocal
