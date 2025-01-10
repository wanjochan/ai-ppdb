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

rem Build infra library first
echo Building infra library...
"%GCC%" %CFLAGS% -c "%SRC_DIR%\infra\infra.c" -o "%BUILD_DIR%\infra.o"
if errorlevel 1 exit /b 1

"%GCC%" %CFLAGS% -c "%SRC_DIR%\infra\infra_platform.c" -o "%BUILD_DIR%\infra_platform.o"
if errorlevel 1 exit /b 1

"%GCC%" %CFLAGS% -c "%SRC_DIR%\infra\infra_sync.c" -o "%BUILD_DIR%\infra_sync.o"
if errorlevel 1 exit /b 1

"%GCC%" %CFLAGS% -c "%SRC_DIR%\infra\infra_async.c" -o "%BUILD_DIR%\infra_async.o"
if errorlevel 1 exit /b 1

rem Build test framework
echo Building test framework...
"%GCC%" %CFLAGS% -c "%TEST_DIR%\white\test_framework.c" -o "%BUILD_DIR%\test_framework.o"
if errorlevel 1 exit /b 1

rem Build infra tests
echo Building infra tests...

rem Define test cases
set "TEST_CASES=test_infra test_async test_sync test_struct test_log test_peer test_memtable test_memory test_metrics test_error test_skiplist"

rem Build and run test cases
set "TEST_TO_RUN=%3"
if "%TEST_TO_RUN%"=="" set "TEST_TO_RUN=%TEST_CASES%"

for %%t in (%TEST_TO_RUN%) do (
    if exist "%TEST_DIR%\white\infra\%%t.c" (
        echo Building %%t...
        "%GCC%" %CFLAGS% "%TEST_DIR%\white\infra\%%t.c" "%BUILD_DIR%\infra.o" "%BUILD_DIR%\infra_platform.o" "%BUILD_DIR%\infra_sync.o" "%BUILD_DIR%\infra_async.o" "%BUILD_DIR%\test_framework.o" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\%%t.exe.dbg"
        if errorlevel 1 exit /b 1
        "%OBJCOPY%" -S -O binary "%BUILD_DIR%\%%t.exe.dbg" "%BUILD_DIR%\%%t.exe"
        if errorlevel 1 exit /b 1

        if not "%2"=="norun" (
            echo Running %%t...
            "%BUILD_DIR%\%%t.exe"
            if errorlevel 1 (
                echo Test %%t failed
                exit /b 1
            )
        )
    ) else (
        echo Error: Test case %%t not found at %TEST_DIR%\white\infra\%%t.c
        exit /b 1
    )
)

exit /b 0 