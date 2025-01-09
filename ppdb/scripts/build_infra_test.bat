@echo off
setlocal EnableDelayedExpansion

rem Load environment variables and common functions
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

rem Create build directory if it doesn't exist
if not exist "%BUILD_DIR%\test" mkdir "%BUILD_DIR%\test"

rem Build infra test
echo Building infra test...

rem Compile infra source files
"%GCC%" %CFLAGS% -c "%SRC_DIR%\infra\infra_core.c" -o "%BUILD_DIR%\test\infra_core.o"
if errorlevel 1 exit /b 1

"%GCC%" %CFLAGS% -c "%SRC_DIR%\infra\infra_struct.c" -o "%BUILD_DIR%\test\infra_struct.o"
if errorlevel 1 exit /b 1

rem Compile test files
"%GCC%" %CFLAGS% -c "%TEST_DIR%\white\infra\test_struct.c" -o "%BUILD_DIR%\test\test_struct.o"
if errorlevel 1 exit /b 1

rem Link test executable
"%GCC%" %LDFLAGS% "%BUILD_DIR%\test\test_struct.o" "%BUILD_DIR%\test\infra_core.o" "%BUILD_DIR%\test\infra_struct.o" %LIBS% -o "%BUILD_DIR%\test\test_struct.com.dbg"
if errorlevel 1 exit /b 1

"%OBJCOPY%" -S -O binary "%BUILD_DIR%\test\test_struct.com.dbg" "%BUILD_DIR%\test\test_struct.com"
if errorlevel 1 exit /b 1

rem Run the test if not explicitly disabled
if not "%1"=="norun" (
    "%BUILD_DIR%\test\test_struct.com"
)

exit /b 0 