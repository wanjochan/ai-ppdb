@echo off
setlocal EnableDelayedExpansion

rem Load environment variables and common functions
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

rem Get test case name from parameter
set "TEST_CASE=%~n1"
if "%TEST_CASE%"=="" (
    echo Error: Please specify a test case to run
    dir %TEST_DIR%\white\infra\*.c
    exit /b 1
)

rem Create build directory if it doesn't exist
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem Build infra library first
echo Building infra library...
"%GCC%" %CFLAGS% -c "%SRC_DIR%\infra\infra.c" -o "%BUILD_DIR%\infra.o"
if errorlevel 1 exit /b 1

"%GCC%" %CFLAGS% -c "%SRC_DIR%\infra\infra_platform.c" -o "%BUILD_DIR%\infra_platform.o"
if errorlevel 1 exit /b 1

"%GCC%" %CFLAGS% -c "%SRC_DIR%\infra\infra_sync.c" -o "%BUILD_DIR%\infra_sync.o"
if errorlevel 1 exit /b 1

@rem "%GCC%" %CFLAGS% -c "%SRC_DIR%\infra\infra_async.c" -o "%BUILD_DIR%\infra_async.o"
@rem if errorlevel 1 exit /b 1

echo Building %TEST_CASE%...

if exist "%TEST_DIR%\white\infra\%TEST_CASE%.c" (
    "%GCC%" %CFLAGS% "%TEST_DIR%\white\infra\%TEST_CASE%.c" "%BUILD_DIR%\infra.o" "%BUILD_DIR%\infra_platform.o" "%BUILD_DIR%\infra_sync.o" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\%TEST_CASE%.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\%TEST_CASE%.exe.dbg" "%BUILD_DIR%\%TEST_CASE%.exe"
    if errorlevel 1 exit /b 1

    echo Running %TEST_CASE%...
    "%BUILD_DIR%\%TEST_CASE%.exe"
    if errorlevel 1 (
        echo Test %TEST_CASE% failed
        exit /b 1
    )
) else (
    echo Error: Test case %TEST_CASE% not found at %TEST_DIR%\white\infra\%TEST_CASE%.c
    exit /b 1
)

exit /b 0 
