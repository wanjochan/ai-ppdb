@echo off
setlocal EnableDelayedExpansion

rem Set paths
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%\.."
set "ROOT_DIR=%CD%"
set "BUILD_DIR=%ROOT_DIR%\build"
set "INCLUDE_DIR=%ROOT_DIR%\include"
set "TEST_DIR=%ROOT_DIR%\test"

rem Get test type
set "TEST_TYPE=%1"
if "%TEST_TYPE%"=="" set "TEST_TYPE=all"

rem Call cosmo_build.bat to setup environment
call "%SCRIPT_DIR%\cosmo_build.bat"
if errorlevel 1 (
    echo Error: Failed to setup Cosmopolitan environment
    exit /b 1
)

rem Set compilation flags
set "CFLAGS=%COMMON_FLAGS% %DEBUG_FLAGS% -nostdinc -I%ROOT_DIR% -I%ROOT_DIR%\include -I%ROOT_DIR%\src -I%ROOT_DIR%\src\kvstore -I%ROOT_DIR%\src\kvstore\internal -I%COSMO% -I%TEST_DIR%\white -include %COSMO%\cosmopolitan.h"
set "LDFLAGS=-static -nostdlib -Wl,-T,%BUILD_DIR%\ape.lds -Wl,--gc-sections -fuse-ld=bfd -Wl,-z,max-page-size=0x1000 -no-pie"
set "LIBS=%BUILD_DIR%\crt.o %BUILD_DIR%\ape.o %BUILD_DIR%\cosmopolitan.a"

rem Main logic
if "%TEST_TYPE%"=="42" (
    call :build_simple_test 42 ""
) else if "%TEST_TYPE%"=="sync" (
    call :build_simple_test sync "src\kvstore\sync.c"
) else (
    if "%TEST_TYPE%"=="unit" (
        echo Building unit tests...
        "%GCC%" %CFLAGS% ^
            src/kvstore/memtable.c ^
            src/kvstore/skiplist.c ^
            src/kvstore/sync.c ^
            test/white/test_basic.c ^
            test/white/test_framework.c ^
            %LDFLAGS% %LIBS% ^
            -o "%BUILD_DIR%\unit_test.exe"
        if errorlevel 1 (
            echo Error: Test build failed
            exit /b 1
        )
        echo Running unit tests...
        "%BUILD_DIR%\unit_test.exe"
    ) else if "%TEST_TYPE%"=="all" (
        echo Building all tests...
        "%GCC%" %CFLAGS% ^
            src/kvstore/memtable.c ^
            src/kvstore/skiplist.c ^
            src/kvstore/sync.c ^
            test/white/test_basic.c ^
            test/white/test_sync.c ^
            test/white/test_42.c ^
            test/white/test_framework.c ^
            %LDFLAGS% %LIBS% ^
            -o "%BUILD_DIR%\all_test.exe"
        if errorlevel 1 (
            echo Error: Test build failed
            exit /b 1
        )
        echo Running all tests...
        "%BUILD_DIR%\all_test.exe"
    ) else (
        echo Error: Unknown test type '%TEST_TYPE%'
        echo Usage: build_test.bat [unit^|42^|sync^|all]
        exit /b 1
    )
)

echo Cleaning up...
exit /b %ERRORLEVEL%

rem Function to build and run a simple test
:build_simple_test
setlocal EnableDelayedExpansion
set "TEST_NAME=%~1"
set "EXTRA_SOURCES=%~2"

echo Building %TEST_NAME% test...
echo Extra sources: [%EXTRA_SOURCES%]
echo Current directory: %CD%

rem First compile extra sources to object files
if not "!EXTRA_SOURCES!"=="" (
    echo.
    echo ===== Compiling extra sources =====
    echo.
    for %%F in (!EXTRA_SOURCES!) do (
        echo Compiling %%F...
        "%GCC%" %CFLAGS% -c "%ROOT_DIR%\%%F" -o "%BUILD_DIR%\%%~nF.o"
        if errorlevel 1 (
            echo Error: Failed to compile %%F
            exit /b 1
        )
    )
)

rem Then compile test file to object file
echo.
echo ===== Compiling test file =====
echo.
"%GCC%" %CFLAGS% -c test/white/test_%TEST_NAME%.c -o "%BUILD_DIR%\test_%TEST_NAME%.o"
if errorlevel 1 (
    echo Error: Failed to compile test file
    exit /b 1
)

rem Finally link everything together
echo.
echo ===== Linking =====
echo.
set "OBJ_FILES="
if not "!EXTRA_SOURCES!"=="" (
    for %%F in (!EXTRA_SOURCES!) do (
        set "OBJ_FILES=!OBJ_FILES! %BUILD_DIR%\%%~nF.o"
    )
)
echo Linking with object files: !OBJ_FILES!
"%GCC%" %LDFLAGS% ^
    "%BUILD_DIR%\test_%TEST_NAME%.o" ^
    !OBJ_FILES! ^
    %LIBS% ^
    -o "%BUILD_DIR%\%TEST_NAME%_test.dbg"

if errorlevel 1 (
    echo Error: Test build failed
    exit /b 1
)

echo Converting to APE format...
echo Command: "%OBJCOPY%" -S -O binary "%BUILD_DIR%\%TEST_NAME%_test.dbg" "%BUILD_DIR%\%TEST_NAME%_test.exe"
"%OBJCOPY%" -S -O binary "%BUILD_DIR%\%TEST_NAME%_test.dbg" "%BUILD_DIR%\%TEST_NAME%_test.exe"
if errorlevel 1 (
    echo Error: objcopy failed
    exit /b 1
)

echo Running %TEST_NAME% test...
echo Command: "%BUILD_DIR%\%TEST_NAME%_test.exe"
"%BUILD_DIR%\%TEST_NAME%_test.exe"
set RESULT=%errorlevel%
echo Exit code: %RESULT%
endlocal & exit /b %RESULT%
