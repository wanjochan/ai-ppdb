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

rem Build infra tests
echo Building infra tests...

rem Define test cases
set "TEST_CASES=test_infra test_infra_async test_infra_sync test_infra_platform"

rem Build and run test cases
set "TEST_TO_RUN=%3"
if "%TEST_TO_RUN%"=="" set "TEST_TO_RUN=%TEST_CASES%"

for %%t in (%TEST_TO_RUN%) do (
    if exist "%TEST_DIR%\infra\%%t.c" (
        echo Building %%t...
        "%GCC%" %CFLAGS% "%TEST_DIR%\infra\%%t.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\%%t.exe.dbg"
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
        echo Error: Test case %%t not found
        exit /b 1
    )
)

exit /b 0 