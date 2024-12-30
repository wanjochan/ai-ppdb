@echo off
rem 设置代码页为 UTF-8
chcp 65001 > nul
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
    echo "  help      显示此帮助信息"
    echo "  ppdb      构建主程序"
    echo "  test42    运行基础测试"
    echo "  sync      运行同步原语测试"
    echo "  skiplist  运行跳表测试"
    echo "  memtable  运行内存表测试"
    echo "  sharded   运行分片内存表测试"
    echo "  kvstore   运行KVStore测试"
    echo "  wal       运行WAL测试"
    echo "  unit      运行单元测试"
    echo "  all       运行所有测试"
    echo.
    echo 构建模式:
    echo "  debug     调试模式 ^(默认^)"
    echo "  release   发布模式"
    echo.
    echo 示例:
    echo "  build.bat help              显示帮助信息"
    echo "  build.bat ppdb              构建主程序"
    echo "  build.bat test42            运行基础测试"
    echo "  build.bat sync debug        以调试模式运行同步测试"
    echo "  build.bat memtable release  以发布模式运行内存表测试"
    echo "  build.bat all               运行所有测试"
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
if "%TEST_TYPE%"=="test42" (
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
) else if "%TEST_TYPE%"=="wal" (
    if "%NEED_REBUILD%"=="1" call :build_simple_test wal "src\kvstore\wal.c src\kvstore\memtable.c src\kvstore\skiplist.c src\kvstore\sync.c src\common\logger.c src\common\error.c src\common\fs.c test\white\test_framework.c" "test\white\storage\test_wal.c"
    if exist "%BUILD_DIR%\wal_test.exe" "%BUILD_DIR%\wal_test.exe"
) else if "%TEST_TYPE%"=="ppdb" (
    echo Building PPDB main program...
    
    rem Compile common modules
    echo Compiling common modules...
    for %%F in (error fs logger) do (
        echo   Compiling src\common\%%F.c...
        "%GCC%" %CFLAGS% -c "%ROOT_DIR%\src\common\%%F.c" -o "%BUILD_DIR%\%%F.o"
        if errorlevel 1 exit /b 1
    )

    rem Compile KVStore modules
    echo Compiling KVStore modules...
    for %%F in (kvstore memtable memtable_iterator metrics monitor sharded_memtable skiplist sync wal kvstore_impl) do (
        echo   Compiling src\kvstore\%%F.c...
        "%GCC%" %CFLAGS% -c "%ROOT_DIR%\src\kvstore\%%F.c" -o "%BUILD_DIR%\%%F.o"
        if errorlevel 1 exit /b 1
    )

    rem Compile main program
    echo Compiling main program...
    "%GCC%" %CFLAGS% -c "%ROOT_DIR%\src\main.c" -o "%BUILD_DIR%\main.o"
    if errorlevel 1 exit /b 1

    rem Create static library
    echo Creating static library...
    "%AR%" rcs "%BUILD_DIR%\libppdb.a" "%BUILD_DIR%\error.o" "%BUILD_DIR%\fs.o" "%BUILD_DIR%\logger.o" "%BUILD_DIR%\kvstore.o" "%BUILD_DIR%\memtable.o" "%BUILD_DIR%\memtable_iterator.o" "%BUILD_DIR%\metrics.o" "%BUILD_DIR%\monitor.o" "%BUILD_DIR%\sharded_memtable.o" "%BUILD_DIR%\skiplist.o" "%BUILD_DIR%\sync.o" "%BUILD_DIR%\wal.o" "%BUILD_DIR%\kvstore_impl.o"
    if errorlevel 1 exit /b 1

    rem Link executable
    echo Linking executable...
    "%GCC%" %LDFLAGS% -o "%BUILD_DIR%\ppdb.exe.dbg" "%BUILD_DIR%\main.o" "%BUILD_DIR%\libppdb.a" %LIBS%
    if errorlevel 1 exit /b 1

    rem Process with objcopy
    echo Processing for cosmopolitan format...
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\ppdb.exe.dbg" "%BUILD_DIR%\ppdb.exe"
    if errorlevel 1 exit /b 1

    echo PPDB build completed successfully
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
) else if "%TEST_TYPE%"=="sharded" (
    if "%NEED_REBUILD%"=="1" call :build_simple_test sharded_memtable "src\kvstore\sharded_memtable.c src\kvstore\memtable.c src\kvstore\skiplist.c src\kvstore\sync.c src\common\logger.c test\white\test_framework.c" "test\white\storage\test_sharded_memtable.c"
    if exist "%BUILD_DIR%\sharded_memtable_test.exe" "%BUILD_DIR%\sharded_memtable_test.exe"
) else if "%TEST_TYPE%"=="kvstore" (
    if "%NEED_REBUILD%"=="1" call :build_simple_test kvstore "src\kvstore\kvstore.c src\kvstore\kvstore_impl.c src\kvstore\sharded_memtable.c src\kvstore\memtable.c src\kvstore\skiplist.c src\kvstore\sync.c src\kvstore\wal.c src\common\logger.c src\common\error.c src\common\fs.c test\white\test_framework.c" "test\white\storage\test_kvstore.c"
    if exist "%BUILD_DIR%\kvstore_test.exe" "%BUILD_DIR%\kvstore_test.exe"
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
