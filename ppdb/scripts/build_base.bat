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

rem Build base tests
echo Building base tests...

rem Build memory test
echo Building memory test...
"%GCC%" %CFLAGS% "%PPDB_DIR%\src\base.c" "%PPDB_DIR%\test\white\infra\test_memory.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\memory_test.exe.dbg"
if errorlevel 1 exit /b 1

"%OBJCOPY%" -S -O binary "%BUILD_DIR%\memory_test.exe.dbg" "%BUILD_DIR%\memory_test.exe"
if errorlevel 1 exit /b 1

rem Run the test if not explicitly disabled
if not "%2"=="norun" (
    "%BUILD_DIR%\memory_test.exe"
)

exit /b 0 