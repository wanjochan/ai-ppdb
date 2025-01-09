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
echo Compiling infra_core.c...
"%GCC%" %CFLAGS% -c "%SRC_DIR%\infra\infra_core.c" -o "%BUILD_DIR%\test\infra_core.o"
if errorlevel 1 exit /b 1

echo Compiling infra_struct.c...
"%GCC%" %CFLAGS% -c "%SRC_DIR%\infra\infra_struct.c" -o "%BUILD_DIR%\test\infra_struct.o"
if errorlevel 1 exit /b 1

echo Compiling infra_sync.c...
"%GCC%" %CFLAGS% -c "%SRC_DIR%\infra\infra_sync.c" -o "%BUILD_DIR%\test\infra_sync.o"
if errorlevel 1 exit /b 1

echo Compiling infra_async.c...
"%GCC%" %CFLAGS% -c "%SRC_DIR%\infra\infra_async.c" -o "%BUILD_DIR%\test\infra_async.o"
if errorlevel 1 exit /b 1

rem Compile test files
echo Compiling test_struct.c...
"%GCC%" %CFLAGS% -c "%TEST_DIR%\white\infra\test_struct.c" -o "%BUILD_DIR%\test\test_struct.o"
if errorlevel 1 exit /b 1

echo Compiling test_sync.c...
"%GCC%" %CFLAGS% -c "%TEST_DIR%\white\infra\test_sync.c" -o "%BUILD_DIR%\test\test_sync.o"
if errorlevel 1 exit /b 1

echo Compiling test_async.c...
"%GCC%" %CFLAGS% -c "%TEST_DIR%\white\infra\test_async.c" -o "%BUILD_DIR%\test\test_async.o"
if errorlevel 1 exit /b 1

rem Link test executables
echo Linking test_struct.com...
"%GCC%" %LDFLAGS% "%BUILD_DIR%\test\test_struct.o" "%BUILD_DIR%\test\infra_core.o" "%BUILD_DIR%\test\infra_struct.o" %LIBS% -o "%BUILD_DIR%\test\test_struct.com.dbg"
if errorlevel 1 exit /b 1

echo Linking test_sync.com...
"%GCC%" %LDFLAGS% "%BUILD_DIR%\test\test_sync.o" "%BUILD_DIR%\test\infra_core.o" "%BUILD_DIR%\test\infra_sync.o" %LIBS% -o "%BUILD_DIR%\test\test_sync.com.dbg"
if errorlevel 1 exit /b 1

echo Linking test_async.com...
"%GCC%" %LDFLAGS% "%BUILD_DIR%\test\test_async.o" "%BUILD_DIR%\test\infra_core.o" "%BUILD_DIR%\test\infra_sync.o" "%BUILD_DIR%\test\infra_async.o" %LIBS% -o "%BUILD_DIR%\test\test_async.com.dbg"
if errorlevel 1 exit /b 1

rem Create binary files
"%OBJCOPY%" -S -O binary "%BUILD_DIR%\test\test_struct.com.dbg" "%BUILD_DIR%\test\test_struct.com"
if errorlevel 1 exit /b 1

"%OBJCOPY%" -S -O binary "%BUILD_DIR%\test\test_sync.com.dbg" "%BUILD_DIR%\test\test_sync.com"
if errorlevel 1 exit /b 1

"%OBJCOPY%" -S -O binary "%BUILD_DIR%\test\test_async.com.dbg" "%BUILD_DIR%\test\test_async.com"
if errorlevel 1 exit /b 1

rem Run the tests if not explicitly disabled
if not "%1"=="norun" (
    echo Running test_struct.com...
    "%BUILD_DIR%\test\test_struct.com"
    if errorlevel 1 exit /b 1

    echo Running test_sync.com...
    "%BUILD_DIR%\test\test_sync.com"
    if errorlevel 1 exit /b 1

    echo Running test_async.com...
    "%BUILD_DIR%\test\test_async.com"
    if errorlevel 1 exit /b 1
)

exit /b 0 