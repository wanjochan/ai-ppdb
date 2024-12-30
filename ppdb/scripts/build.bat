@echo off
setlocal EnableDelayedExpansion

rem Set paths
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%\.."
set "ROOT_DIR=%CD%"
set "BUILD_DIR=%ROOT_DIR%\build"
set "INCLUDE_DIR=%ROOT_DIR%\include"
set "TEST_DIR=%ROOT_DIR%\test"

rem Get test type and build mode
set "TEST_TYPE=%1"
set "BUILD_MODE=%2"
if "%TEST_TYPE%"=="" set "TEST_TYPE=help"
if "%BUILD_MODE%"=="" set "BUILD_MODE=debug"

rem Show help if requested
if "%TEST_TYPE%"=="help" (
    echo PPDB 构建和测试工具
    echo.
    echo 用法: build.bat [模块] [构建模式]
    echo.
    echo 可用模块:
    echo   help      显示此帮助信息
    echo   42        运行基础测试
    echo   sync      运行同步原语测试
    echo   skiplist  运行跳表测试
    echo   memtable  运行内存表测试
    echo   unit      运行单元测试
    echo   all       运行所有测试
    echo.
    echo 构建模式:
    echo   debug     调试模式 ^(默认^)
    echo   release   发布模式
    echo.
    echo 示例:
    echo   build.bat help              显示帮助信息
    echo   build.bat 42                运行基础测试
    echo   build.bat sync debug        以调试模式运行同步测试
    echo   build.bat memtable release  以发布模式运行内存表测试
    echo   build.bat all               运行所有测试
    exit /b 0
)

rem Call cosmo_build.bat to setup environment
call "%SCRIPT_DIR%\cosmo_build.bat"
if errorlevel 1 (
    echo Error: Failed to setup Cosmopolitan environment
    exit /b 1
)

rem Set compilation flags based on build mode
if /i "%BUILD_MODE%"=="release" (
    set "BUILD_FLAGS=%COMMON_FLAGS% %RELEASE_FLAGS%"
) else (
    set "BUILD_FLAGS=%COMMON_FLAGS% %DEBUG_FLAGS%"
)

rem Set include paths
set "INCLUDE_FLAGS=-nostdinc -I%ROOT_DIR% -I%ROOT_DIR%\include -I%ROOT_DIR%\src -I%ROOT_DIR%\src\kvstore -I%ROOT_DIR%\src\kvstore\internal -I%COSMO% -I%TEST_DIR%\white"

rem Set final CFLAGS
set "CFLAGS=%BUILD_FLAGS% %INCLUDE_FLAGS% -include %COSMO%\cosmopolitan.h"

rem Set linker flags
set "LDFLAGS=-static -nostdlib -Wl,-T,%BUILD_DIR%\ape.lds -Wl,--gc-sections -fuse-ld=bfd -Wl,-z,max-page-size=0x1000 -no-pie"
set "LIBS=%BUILD_DIR%\crt.o %BUILD_DIR%\ape.o %BUILD_DIR%\cosmopolitan.a"

rem Check if we need to rebuild test
set "NEED_REBUILD=0"
if not exist "%BUILD_DIR%\%TEST_TYPE%_test.exe" set "NEED_REBUILD=1"

rem Main logic
if "%TEST_TYPE%"=="42" (
    if "%NEED_REBUILD%"=="1" call :build_simple_test 42 ""
    if exist "%BUILD_DIR%\42_test.exe" "%BUILD_DIR%\42_test.exe"
) else if "%TEST_TYPE%"=="sync" (
    if "%NEED_REBUILD%"=="1" call :build_simple_test sync "src\kvstore\sync.c src\common\logger.c src\common\error.c test\white\test_framework.c" "test\white\infra\test_sync.c"
    if exist "%BUILD_DIR%\sync_test.exe" "%BUILD_DIR%\sync_test.exe"
) else if "%TEST_TYPE%"=="skiplist" (
    if "%NEED_REBUILD%"=="1" call :build_simple_test skiplist "src\kvstore\skiplist.c src\kvstore\sync.c src\common\logger.c"
    if exist "%BUILD_DIR%\skiplist_test.exe" "%BUILD_DIR%\skiplist_test.exe"
) else if "%TEST_TYPE%"=="memtable" (
    if "%NEED_REBUILD%"=="1" call :build_simple_test memtable "src\kvstore\memtable.c src\kvstore\skiplist.c src\kvstore\sync.c src\common\logger.c test\white\test_framework.c" "test\white\storage\test_memtable.c"
    if exist "%BUILD_DIR%\memtable_test.exe" "%BUILD_DIR%\memtable_test.exe"
) else if "%TEST_TYPE%"=="unit" (
    if "%NEED_REBUILD%"=="1" (
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
    )
    echo Running unit tests...
    "%BUILD_DIR%\unit_test.exe"
) else if "%TEST_TYPE%"=="all" (
    if "%NEED_REBUILD%"=="1" (
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
    )
    echo Running all tests...
    "%BUILD_DIR%\all_test.exe"
) else (
    echo Error: Unknown module '%TEST_TYPE%'
    echo 运行 'build.bat help' 查看帮助信息
    exit /b 1
)

echo Cleaning up...
exit /b %ERRORLEVEL%

rem Function to check if source is newer than object
:check_need_rebuild
setlocal EnableDelayedExpansion
set "SRC_FILE=%~1"
set "OBJ_FILE=%~2"

if not exist "!OBJ_FILE!" (
    endlocal & exit /b 1
)

for %%A in ("!SRC_FILE!") do set "SRC_TIME=%%~tA"
for %%A in ("!OBJ_FILE!") do set "OBJ_TIME=%%~tA"

if "!SRC_TIME!" gtr "!OBJ_TIME!" (
    endlocal & exit /b 1
) else (
    endlocal & exit /b 0
)

rem Function to build and run a simple test
:build_simple_test
setlocal EnableDelayedExpansion
set "TEST_NAME=%~1"
set "EXTRA_SOURCES=%~2"
set "TEST_FILE=%~3"

echo Building %TEST_NAME% test...
echo Extra sources: [%EXTRA_SOURCES%]
echo Current directory: %CD%

rem First compile extra sources to object files
if not "!EXTRA_SOURCES!"=="" (
    echo.
    echo ===== Compiling extra sources =====
    echo.
    for %%F in (!EXTRA_SOURCES!) do (
        set "OBJ_FILE=%BUILD_DIR%\%%~nF.o"
        call :check_need_rebuild "%ROOT_DIR%\%%F" "!OBJ_FILE!"
        if !errorlevel! equ 1 (
            echo Compiling %%F...
            "%GCC%" %CFLAGS% -c "%ROOT_DIR%\%%F" -o "!OBJ_FILE!"
            if errorlevel 1 (
                echo Error: Failed to compile %%F
                exit /b 1
            )
        ) else (
            echo Object file !OBJ_FILE! is up to date
        )
    )
)

rem Then compile test file to object file
echo.
echo ===== Compiling test file =====
echo.
set "TEST_OBJ=%BUILD_DIR%\test_%TEST_NAME%.o"
if not "!TEST_FILE!"=="" (
    set "TEST_SRC=!TEST_FILE!"
) else (
    set "TEST_SRC=test/white/test_%TEST_NAME%.c"
)
call :check_need_rebuild "!TEST_SRC!" "!TEST_OBJ!"
if !errorlevel! equ 1 (
    echo Compiling !TEST_SRC!...
    "%GCC%" %CFLAGS% -c "!TEST_SRC!" -o "!TEST_OBJ!"
    if errorlevel 1 (
        echo Error: Failed to compile test file
        exit /b 1
    )
) else (
    echo Test object file !TEST_OBJ! is up to date
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

endlocal & exit /b 0 