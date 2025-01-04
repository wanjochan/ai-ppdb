@echo off
rem ===== Note: This batch file uses English only to avoid Windows encoding issues =====
chcp 65001 > nul
setlocal EnableDelayedExpansion

rem ===== Set Environment Variables =====
call :set_environment
if errorlevel 1 exit /b 1

rem ===== Get Command Line Parameters =====
set "TEST_TYPE=%1"
set "BUILD_MODE=%2"
if "%TEST_TYPE%"=="" set "TEST_TYPE=help"
if "%BUILD_MODE%"=="" set "BUILD_MODE=release"

rem ===== Validate Build Mode =====
if /i not "%BUILD_MODE%"=="debug" if /i not "%BUILD_MODE%"=="release" (
    echo Invalid build mode: %BUILD_MODE%
    echo Valid modes are: debug, release
    exit /b 1
)

rem ===== Display Help Information =====
if "%TEST_TYPE%"=="help" (
    call :show_help
    exit /b 0
)

rem ===== Clean Command =====
if "%TEST_TYPE%"=="clean" (
    call :clean_build
    exit /b 0
)

rem ===== Force Rebuild =====
if "%TEST_TYPE%"=="rebuild" (
    call :build_rebuild
    exit /b 0
)

rem ===== Build Base Storage =====
if "%TEST_TYPE%"=="base" (
    call :build_base_storage
    exit /b 0
)

rem ===== Check Environment =====
call :check_runtime_files
if errorlevel 1 exit /b 1

call :check_header_changes
if errorlevel 1 exit /b 1

rem ===== Set Compile Flags =====
call :set_build_flags
if errorlevel 1 exit /b 1

rem ===== Execute Build =====
call :build_%TEST_TYPE%
exit /b %ERRORLEVEL%

rem ===== Helper Functions =====
:set_environment
    rem Set proxy if provided
    set "PROXY="
    if not "%HTTP_PROXY%"=="" (
        set "PROXY=%HTTP_PROXY%"
    ) else if not "%HTTPS_PROXY%"=="" (
        set "PROXY=%HTTPS_PROXY%"
    )

    if not "%PROXY%"=="" (
        echo Using proxy: %PROXY%
    )

    rem Set paths (using absolute paths)
    set "SCRIPT_DIR=%~dp0"
    set "ROOT_DIR=%SCRIPT_DIR%\.."
    pushd "%ROOT_DIR%"
    set "ROOT_DIR=%CD%"
    popd
    set "PPDB_DIR=%ROOT_DIR%"
    set "BUILD_DIR=%PPDB_DIR%\build"
    set "INCLUDE_DIR=%PPDB_DIR%\include"
    set "TEST_DIR=%PPDB_DIR%\test"

    rem Set tool paths
    set "COSMO=%ROOT_DIR%\..\repos\cosmopolitan"
    set "CROSS9=%ROOT_DIR%\..\repos\cross9\bin"
    set "GCC=%CROSS9%\x86_64-pc-linux-gnu-gcc.exe"
    set "AR=%CROSS9%\x86_64-pc-linux-gnu-ar.exe"
    set "OBJCOPY=%CROSS9%\x86_64-pc-linux-gnu-objcopy.exe"

    rem Verify tool paths
    if not exist "%GCC%" (
        echo Error: GCC not found at %GCC%
        exit /b 1
    )
    if not exist "%AR%" (
        echo Error: AR not found at %AR%
        exit /b 1
    )
    if not exist "%OBJCOPY%" (
        echo Error: OBJCOPY not found at %OBJCOPY%
        exit /b 1
    )
    exit /b 0

:show_help
    echo PPDB Build and Test Tool
    echo.
    echo Usage: build.bat [module] [build mode]
    echo.
    echo Available modules:
    echo   test42            Run basic tests
    echo   sync_locked       Run locked synchronization tests
    echo   sync_lockfree     Run lock-free synchronization tests
    echo   skiplist_locked   Run locked skiplist tests
    echo   skiplist_lockfree Run lock-free skiplist tests
    echo   memtable_locked   Run locked memtable tests
    echo   memtable_lockfree Run lock-free memtable tests
    echo   sharded_locked    Run sharded memtable tests
    echo   sharded_lockfree  Run sharded memtable tests
    echo.
    echo   kvstore          Run KVStore tests
    echo   wal_core         Run WAL core tests
    echo   wal_func         Run WAL function tests
    echo   wal_advanced     Run WAL advanced tests
    echo   clean            Clean build directory
    echo   rebuild          Force rebuild
    echo   ppdb             Build PPDB
    echo.
    echo Build modes:
    echo   debug     Debug mode
    echo   release   Release mode (default)
    echo.
    echo Examples:
    echo   build.bat help                Display help information
    echo   build.bat clean               Clean build directory
    echo   build.bat rebuild             Force rebuild
    echo   build.bat test42              Run basic tests
    echo   build.bat sync_locked debug   Run locked synchronization tests in debug mode
    echo   build.bat sync_lockfree       Run lock-free synchronization tests in release mode
    exit /b 0

:clean_build
    echo Cleaning build directory...
    del /F /Q "%BUILD_DIR%\*.o"
    del /F /Q "%BUILD_DIR%\*.exe"
    del /F /Q "%BUILD_DIR%\*.exe.dbg"
    echo Restoring runtime files...
    copy /Y "%COSMO%\crt.o" "%BUILD_DIR%\" > nul
    copy /Y "%COSMO%\ape.o" "%BUILD_DIR%\" > nul
    copy /Y "%COSMO%\ape.lds" "%BUILD_DIR%\" > nul
    copy /Y "%COSMO%\cosmopolitan.a" "%BUILD_DIR%\" > nul
    exit /b 0

:check_runtime_files
    if not exist "%BUILD_DIR%\crt.o" (
        echo Error: Missing runtime files. Please run setup.bat first to set up the environment
        echo Usage: .\setup.bat
        exit /b 1
    )
    if not exist "%BUILD_DIR%\ape.o" (
        echo Error: Missing runtime files. Please run setup.bat first to set up the environment
        echo Usage: .\setup.bat
        exit /b 1
    )
    if not exist "%BUILD_DIR%\cosmopolitan.a" (
        echo Error: Missing runtime files. Please run setup.bat first to set up the environment
        echo Usage: .\setup.bat
        exit /b 1
    )
    if not exist "%BUILD_DIR%\ape.lds" (
        echo Error: Missing runtime files. Please run setup.bat first to set up the environment
        echo Usage: .\setup.bat
        exit /b 1
    )
    exit /b 0

:check_header_changes
    for %%F in (%INCLUDE_DIR%\ppdb\*.h) do (
        if %%~tF gtr %BUILD_DIR%\*.o (
            echo Header %%F has been updated, rebuilding...
            del /F /Q "%BUILD_DIR%\*.o"
            exit /b 1
        )
    )
    exit /b 0

:set_build_flags
    if /i "%BUILD_MODE%"=="release" (
        set "COMMON_FLAGS=-g -O2 -Wall -Wextra -fno-pie -fno-stack-protector -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -fno-asynchronous-unwind-tables"
        set "RELEASE_FLAGS=-DNDEBUG"
        set "BUILD_FLAGS=%COMMON_FLAGS% %RELEASE_FLAGS%"
    ) else (
        set "COMMON_FLAGS=-g -O0 -Wall -Wextra -fno-pie -fno-stack-protector -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -fno-asynchronous-unwind-tables"
        set "DEBUG_FLAGS=-DDEBUG"
        set "BUILD_FLAGS=%COMMON_FLAGS% %DEBUG_FLAGS%"
    )

    rem Set include paths
    set "INCLUDE_FLAGS=-nostdinc -I%PPDB_DIR% -I%PPDB_DIR%\include -I%PPDB_DIR%\src -I%COSMO% -I%TEST_DIR%\white -I%CROSS9%\..\x86_64-pc-linux-gnu\include"

    rem Set final CFLAGS
    set "CFLAGS=%BUILD_FLAGS% %INCLUDE_FLAGS% -include %COSMO%\cosmopolitan.h"
    
    rem Set sync mode if specified
    if "%PPDB_SYNC_MODE%"=="lockfree" (
        set "CFLAGS=%CFLAGS% -DPPDB_SYNC_MODE_LOCKFREE"
    )

    rem Set linker flags
    set "LDFLAGS=-static -nostdlib -Wl,-T,%BUILD_DIR%\ape.lds -Wl,--gc-sections -B%CROSS9% -Wl,-z,max-page-size=0x1000 -no-pie"
    set "LIBS=%BUILD_DIR%\crt.o %BUILD_DIR%\ape.o %BUILD_DIR%\cosmopolitan.a"
    exit /b 0

:build_test42
    echo Building test42...
    "%GCC%" %CFLAGS% "%PPDB_DIR%\test\white\test_42.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\42_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\42_test.exe.dbg" "%BUILD_DIR%\42_test.exe"
    if errorlevel 1 exit /b 1
    "%BUILD_DIR%\42_test.exe"
    exit /b 0
:build_sync_locked
    echo Building sync locked test...
    set "PPDB_SYNC_MODE=locked"
    "%GCC%" %CFLAGS% ^
        "%PPDB_DIR%\src\base.c" ^
        "%PPDB_DIR%\test\white\test_framework.c" ^
        "%PPDB_DIR%\test\white\infra\test_sync.c" ^
        %LDFLAGS% %LIBS% -o "%BUILD_DIR%\sync_locked_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\sync_locked_test.exe.dbg" "%BUILD_DIR%\sync_locked_test.exe"
    if errorlevel 1 exit /b 1
    "%BUILD_DIR%\sync_locked_test.exe"
    exit /b 0

:build_sync_lockfree
    echo Building sync lockfree test...
    set "PPDB_SYNC_MODE=lockfree"
    "%GCC%" %CFLAGS% ^
        "%PPDB_DIR%\src\base.c" ^
        "%PPDB_DIR%\test\white\test_framework.c" ^
        "%PPDB_DIR%\test\white\infra\test_sync.c" ^
        %LDFLAGS% %LIBS% -o "%BUILD_DIR%\sync_lockfree_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\sync_lockfree_test.exe.dbg" "%BUILD_DIR%\sync_lockfree_test.exe"
    if errorlevel 1 exit /b 1
    "%BUILD_DIR%\sync_lockfree_test.exe"
    exit /b 0

:build_skiplist_locked
    echo Building skiplist locked test...
    set "PPDB_SYNC_MODE=locked"
    "%GCC%" %CFLAGS% ^
        "%PPDB_DIR%\src\base.c" ^
        "%PPDB_DIR%\src\storage.c" ^
        "%PPDB_DIR%\test\white\test_framework.c" ^
        "%PPDB_DIR%\test\white\infra\test_skiplist.c" ^
        %LDFLAGS% %LIBS% -o "%BUILD_DIR%\skiplist_locked_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\skiplist_locked_test.exe.dbg" "%BUILD_DIR%\skiplist_locked_test.exe"
    if errorlevel 1 exit /b 1
    "%BUILD_DIR%\skiplist_locked_test.exe"
    exit /b 0

:build_skiplist_lockfree
    echo Building skiplist lockfree test...
    set "PPDB_SYNC_MODE=lockfree"
    "%GCC%" %CFLAGS% ^
        "%PPDB_DIR%\src\base.c" ^
        "%PPDB_DIR%\src\storage.c" ^
        "%PPDB_DIR%\test\white\test_framework.c" ^
        "%PPDB_DIR%\test\white\infra\test_skiplist.c" ^
        %LDFLAGS% %LIBS% -o "%BUILD_DIR%\skiplist_lockfree_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\skiplist_lockfree_test.exe.dbg" "%BUILD_DIR%\skiplist_lockfree_test.exe"
    if errorlevel 1 exit /b 1
    "%BUILD_DIR%\skiplist_lockfree_test.exe"
    exit /b 0

:build_memtable_locked
    echo Building memtable locked test...
    set "PPDB_SYNC_MODE=locked"
    "%GCC%" %CFLAGS% ^
        "%PPDB_DIR%\src\base.c" ^
        "%PPDB_DIR%\src\storage.c" ^
        "%PPDB_DIR%\test\white\test_framework.c" ^
        "%PPDB_DIR%\test\white\infra\test_memtable.c" ^
        %LDFLAGS% %LIBS% -o "%BUILD_DIR%\memtable_locked_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\memtable_locked_test.exe.dbg" "%BUILD_DIR%\memtable_locked_test.exe"
    if errorlevel 1 exit /b 1
    "%BUILD_DIR%\memtable_locked_test.exe"
    exit /b 0

:build_memtable_lockfree
    echo Building memtable lockfree test...
    set "PPDB_SYNC_MODE=lockfree"
    call :set_build_flags
    "%GCC%" %CFLAGS% ^
        "%PPDB_DIR%\src\base.c" ^
        "%PPDB_DIR%\src\storage.c" ^
        "%PPDB_DIR%\test\white\test_framework.c" ^
        "%PPDB_DIR%\test\white\infra\test_memtable.c" ^
        %LDFLAGS% %LIBS% -o "%BUILD_DIR%\memtable_lockfree_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\memtable_lockfree_test.exe.dbg" "%BUILD_DIR%\memtable_lockfree_test.exe"
    if errorlevel 1 exit /b 1
    "%BUILD_DIR%\memtable_lockfree_test.exe"
    exit /b 0

:build_sharded
    echo Building sharded test...
    "%GCC%" %CFLAGS% ^
        "%PPDB_DIR%\src\base.c" ^
        "%PPDB_DIR%\src\storage.c" ^
        "%PPDB_DIR%\test\white\test_framework.c" ^
        "%PPDB_DIR%\test\white\infra\test_sharded.c" ^
        %LDFLAGS% %LIBS% -o "%BUILD_DIR%\sharded_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\sharded_test.exe.dbg" "%BUILD_DIR%\sharded_test.exe"
    if errorlevel 1 exit /b 1
    "%BUILD_DIR%\sharded_test.exe"
    exit /b 0

:build_kvstore
    echo Building kvstore test...
    "%GCC%" %CFLAGS% ^
        "%PPDB_DIR%\src\base.c" ^
        "%PPDB_DIR%\src\storage.c" ^
        "%PPDB_DIR%\src\ppdb.c" ^
        "%PPDB_DIR%\test\white\test_framework.c" ^
        "%PPDB_DIR%\test\white\infra\test_kvstore.c" ^
        %LDFLAGS% %LIBS% -o "%BUILD_DIR%\kvstore_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\kvstore_test.exe.dbg" "%BUILD_DIR%\kvstore_test.exe"
    if errorlevel 1 exit /b 1
    "%BUILD_DIR%\kvstore_test.exe"
    exit /b 0

:build_rebuild
    echo.
    echo ===== Force Rebuild =====
    echo Checking runtime files...
    call :check_runtime_files
    if errorlevel 1 exit /b 1
    echo Cleaning compile files...
    del /F /Q "%BUILD_DIR%\*.o"
    echo Restoring runtime files...
    copy /Y "%COSMO%\crt.o" "%BUILD_DIR%\" > nul
    copy /Y "%COSMO%\ape.o" "%BUILD_DIR%\" > nul
    copy /Y "%COSMO%\ape.lds" "%BUILD_DIR%\" > nul
    copy /Y "%COSMO%\cosmopolitan.a" "%BUILD_DIR%\" > nul
    echo Cleaning executable files...
    del /F /Q "%BUILD_DIR%\*.exe"
    del /F /Q "%BUILD_DIR%\*.exe.dbg"
    echo Rebuild complete
    echo.
    exit /b 0 
:help
@echo Usage: build.bat [target]
@echo Available targets:
@echo   clean       - Clean build directory
@echo   rebuild     - Force rebuild all
@echo   test42     - Run test 42
@echo   sync_locked   - Build and run sync test with locks
@echo   sync_lockfree - Build and run sync test without locks
@echo   skiplist_locked   - Build and run skiplist test with locks
@echo   skiplist_lockfree - Build and run skiplist test without locks
@echo   memtable   - Build and run memtable test
@echo   sharded    - Build and run sharded test
@echo   ppdb       - Build and run ppdb test
goto :eof

:main
if "%1"=="" goto help
if "%1"=="help" goto show_help
if "%1"=="clean" goto clean_build
if "%1"=="rebuild" goto build_rebuild
if "%1"=="test42" goto build_test42
if "%1"=="sync_locked" goto build_sync_locked
if "%1"=="sync_lockfree" goto build_sync_lockfree
if "%1"=="skiplist" goto build_skiplist
if "%1"=="skiplist_locked" goto build_skiplist_locked
if "%1"=="skiplist_lockfree" goto build_skiplist_lockfree
if "%1"=="memtable" goto build_memtable
if "%1"=="memtable_locked" goto build_memtable_locked
if "%1"=="memtable_lockfree" goto build_memtable_lockfree
if "%1"=="sharded_locked" goto build_sharded_locked
if "%1"=="sharded_lockfree" goto build_sharded_lockfree
if "%1"=="ppdb" goto build_ppdb
if "%1"=="kvstore" goto build_kvstore
if "%1"=="wal_core" goto build_wal_core
if "%1"=="wal_func" goto build_wal_func
if "%1"=="wal_advanced" goto build_wal_advanced
if "%1"=="base" goto build_base_storage
goto show_help 

:build_base_storage
echo Building base storage...
%CC% %CFLAGS% %INCLUDE_DIRS% ^
    ../src/storage.c ^
    ../src/iterator.c ^
    ../src/sync.c ^
    ../src/utils.c ^
    -o ../bin/ppdb.dll
if errorlevel 1 exit /b 1
echo Base storage built successfully
exit /b 0

:build_skiplist_test
echo Building skiplist test...
%CC% %CFLAGS% %INCLUDE_DIRS% ^
    ../test/white/infra/test_skiplist.c ^
    ../src/storage.c ^
    ../src/iterator.c ^
    ../src/sync.c ^
    ../src/utils.c ^
    -o ../bin/test_skiplist.exe
if errorlevel 1 exit /b 1
echo Skiplist test built successfully
exit /b 0 

:build_sharded_locked
    echo Building sharded memtable test (locked mode)...
    set "PPDB_SYNC_MODE=locked"
    "%GCC%" %CFLAGS% ^
        "%PPDB_DIR%\src\base.c" ^
        "%PPDB_DIR%\src\storage.c" ^
        "%PPDB_DIR%\test\white\test_framework.c" ^
        "%PPDB_DIR%\test\white\storage\test_sharded_memtable.c" ^
        %LDFLAGS% %LIBS% -o "%BUILD_DIR%\sharded_locked_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\sharded_locked_test.exe.dbg" "%BUILD_DIR%\sharded_locked_test.exe"
    if errorlevel 1 exit /b 1
    "%BUILD_DIR%\sharded_locked_test.exe"
    exit /b 0

:build_sharded_lockfree
    echo Building sharded memtable test (lockfree mode)...
    set "PPDB_SYNC_MODE=lockfree"
    "%GCC%" %CFLAGS% ^
        "%PPDB_DIR%\src\base.c" ^
        "%PPDB_DIR%\src\storage.c" ^
        "%PPDB_DIR%\test\white\test_framework.c" ^
        "%PPDB_DIR%\test\white\storage\test_sharded_memtable.c" ^
        %LDFLAGS% %LIBS% -o "%BUILD_DIR%\sharded_lockfree_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\sharded_lockfree_test.exe.dbg" "%BUILD_DIR%\sharded_lockfree_test.exe"
    if errorlevel 1 exit /b 1
    "%BUILD_DIR%\sharded_lockfree_test.exe"
    exit /b 0 
