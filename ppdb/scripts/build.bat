@echo off
chcp 65001 > nul
setlocal EnableDelayedExpansion

rem ===== 设置环境变量 =====
call :set_environment
if errorlevel 1 exit /b 1

rem ===== 获取命令行参数 =====
set "TEST_TYPE=%1"
set "BUILD_MODE=%2"
if "%TEST_TYPE%"=="" set "TEST_TYPE=help"
if "%BUILD_MODE%"=="" set "BUILD_MODE=debug"

rem ===== 显示帮助信息 =====
if "%TEST_TYPE%"=="help" (
    call :show_help
    exit /b 0
)

rem ===== 清理命令 =====
if "%TEST_TYPE%"=="clean" (
    call :clean_build
    exit /b 0
)

rem ===== 强制重新构建 =====
if "%TEST_TYPE%"=="rebuild" (
    call :build_rebuild
    exit /b 0
)

rem ===== 检查环境 =====
call :check_runtime_files
if errorlevel 1 exit /b 1

call :check_header_changes
if errorlevel 1 exit /b 1

rem ===== 设置编译标志 =====
call :set_build_flags
if errorlevel 1 exit /b 1

rem ===== 执行构建 =====
call :build_%TEST_TYPE%
exit /b %ERRORLEVEL%

rem ===== 辅助函数 =====
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

    rem Set paths (使用绝对路径)
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
    echo PPDB 构建和测试工具
    echo.
    echo 用法: build.bat [模块] [构建模式]
    echo.
    echo 可用模块:
    echo "  test42    运行基础测试"
    echo "  sync_locked      运行有锁同步原语测试"
    echo "  sync_lockfree    运行无锁同步原语测试"
    echo "  skiplist  运行跳表测试"
    echo "  memtable  运行内存表测试"
    echo "  sharded   运行分片内存表测试"
    echo "  ppdb_memkv 构建纯内存KV版本"
    echo.
    echo "  kvstore   运行KVStore测试"
    echo "  wal_core  运行WAL核心测试"
    echo "  wal_func  运行WAL功能测试"
    echo "  wal_advanced 运行WAL高级测试"
    echo "  clean     清理构建目录"
    echo "  rebuild   强制重新构建"
    echo.
    echo 构建模式:
    echo "  debug     调试模式 ^(默认^)"
    echo "  release   发布模式"
    echo.
    echo 示例:
    echo "  build.bat help              显示帮助信息"
    echo "  build.bat clean             清理构建目录"
    echo "  build.bat rebuild           强制重新构建"
    echo "  build.bat test42            运行基础测试"
    echo "  build.bat sync debug        以调试模式运行同步测试"
    echo "  build.bat memtable release  以发布模式运行内存表测试"
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
        echo 错误：缺少运行时文件，请先运行 setup.bat 进行环境设置
        echo 用法：.\setup.bat
        exit /b 1
    )
    if not exist "%BUILD_DIR%\ape.o" (
        echo 错误：缺少运行时文件，请先运行 setup.bat 进行环境设置
        echo 用法：.\setup.bat
        exit /b 1
    )
    if not exist "%BUILD_DIR%\cosmopolitan.a" (
        echo 错误：缺少运行时文件，请先运行 setup.bat 进行环境设置
        echo 用法：.\setup.bat
        exit /b 1
    )
    if not exist "%BUILD_DIR%\ape.lds" (
        echo 错误：缺少运行时文件，请先运行 setup.bat 进行环境设置
        echo 用法：.\setup.bat
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
    set "INCLUDE_FLAGS=-nostdinc -I%PPDB_DIR% -I%PPDB_DIR%\include -I%PPDB_DIR%\src -I%PPDB_DIR%\src\kvstore -I%PPDB_DIR%\src\kvstore\internal -I%PPDB_DIR%\src\common -I%COSMO% -I%TEST_DIR%\white -I%CROSS9%\..\x86_64-pc-linux-gnu\include"

    rem Set final CFLAGS
    set "CFLAGS=%BUILD_FLAGS% %INCLUDE_FLAGS% -include %COSMO%\cosmopolitan.h"

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
    "%GCC%" %CFLAGS% -DPPDB_SYNC_USE_LOCK=1 ^
        "%PPDB_DIR%\src\kvstore\sync.c" ^
        "%PPDB_DIR%\src\common\logger.c" ^
        "%PPDB_DIR%\src\common\error.c" ^
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
    "%GCC%" %CFLAGS% -DPPDB_SYNC_USE_LOCK=0 ^
        "%PPDB_DIR%\src\kvstore\sync.c" ^
        "%PPDB_DIR%\src\common\logger.c" ^
        "%PPDB_DIR%\src\common\error.c" ^
        "%PPDB_DIR%\test\white\test_framework.c" ^
        "%PPDB_DIR%\test\white\infra\test_sync.c" ^
        %LDFLAGS% %LIBS% -o "%BUILD_DIR%\sync_lockfree_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\sync_lockfree_test.exe.dbg" "%BUILD_DIR%\sync_lockfree_test.exe"
    if errorlevel 1 exit /b 1
    "%BUILD_DIR%\sync_lockfree_test.exe"
    exit /b 0

:build_skiplist
    echo Building skiplist test...
    "%GCC%" %CFLAGS% ^
        "%PPDB_DIR%\src\kvstore\skiplist.c" ^
        "%PPDB_DIR%\src\kvstore\sync.c" ^
        "%PPDB_DIR%\src\common\logger.c" ^
        "%PPDB_DIR%\test\white\test_framework.c" ^
        "%PPDB_DIR%\test\white\test_skiplist.c" ^
        %LDFLAGS% %LIBS% -o "%BUILD_DIR%\skiplist_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\skiplist_test.exe.dbg" "%BUILD_DIR%\skiplist_test.exe"
    if errorlevel 1 exit /b 1
    "%BUILD_DIR%\skiplist_test.exe"
    exit /b 0

:build_memtable
    echo Building memtable test...
    "%GCC%" %CFLAGS% ^
        "%PPDB_DIR%\src\kvstore\memtable.c" ^
        "%PPDB_DIR%\src\kvstore\skiplist.c" ^
        "%PPDB_DIR%\src\kvstore\sync.c" ^
        "%PPDB_DIR%\src\common\logger.c" ^
        "%PPDB_DIR%\test\white\test_framework.c" ^
        "%PPDB_DIR%\test\white\storage\test_memtable.c" ^
        %LDFLAGS% %LIBS% -o "%BUILD_DIR%\memtable_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\memtable_test.exe.dbg" "%BUILD_DIR%\memtable_test.exe"
    if errorlevel 1 exit /b 1
    "%BUILD_DIR%\memtable_test.exe"
    exit /b 0

:build_sharded
    echo Building sharded test...
    "%GCC%" %CFLAGS% ^
        "%PPDB_DIR%\src\kvstore\sharded_memtable.c" ^
        "%PPDB_DIR%\src\kvstore\memtable.c" ^
        "%PPDB_DIR%\src\kvstore\skiplist.c" ^
        "%PPDB_DIR%\src\kvstore\sync.c" ^
        "%PPDB_DIR%\src\common\logger.c" ^
        "%PPDB_DIR%\test\white\test_framework.c" ^
        "%PPDB_DIR%\test\white\storage\test_sharded_memtable.c" ^
        %LDFLAGS% %LIBS% -o "%BUILD_DIR%\sharded_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\sharded_test.exe.dbg" "%BUILD_DIR%\sharded_test.exe"
    if errorlevel 1 exit /b 1
    "%BUILD_DIR%\sharded_test.exe"
    exit /b 0

:build_wal_core
    echo Building WAL core test...
    "%GCC%" %CFLAGS% ^
        "%PPDB_DIR%\src\kvstore\wal.c" ^
        "%PPDB_DIR%\src\kvstore\wal_write.c" ^
        "%PPDB_DIR%\src\kvstore\sync.c" ^
        "%PPDB_DIR%\src\common\logger.c" ^
        "%PPDB_DIR%\src\common\error.c" ^
        "%PPDB_DIR%\src\common\fs.c" ^
        "%PPDB_DIR%\test\white\test_framework.c" ^
        "%PPDB_DIR%\src\kvstore\memtable.c" ^
        "%PPDB_DIR%\src\kvstore\skiplist.c" ^
        "%PPDB_DIR%\test\white\storage\test_wal_core.c" ^
        %LDFLAGS% %LIBS% -o "%BUILD_DIR%\wal_core_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\wal_core_test.exe.dbg" "%BUILD_DIR%\wal_core_test.exe"
    if errorlevel 1 exit /b 1
    "%BUILD_DIR%\wal_core_test.exe"
    exit /b 0

:build_wal_func
    echo Building WAL functional test...
    "%GCC%" %CFLAGS% ^
        "%PPDB_DIR%\src\kvstore\wal.c" ^
        "%PPDB_DIR%\src\kvstore\wal_write.c" ^
        "%PPDB_DIR%\src\kvstore\wal_iterator.c" ^
        "%PPDB_DIR%\src\kvstore\sync.c" ^
        "%PPDB_DIR%\src\common\logger.c" ^
        "%PPDB_DIR%\src\common\error.c" ^
        "%PPDB_DIR%\src\common\fs.c" ^
        "%PPDB_DIR%\test\white\test_framework.c" ^
        "%PPDB_DIR%\test\white\storage\test_wal_func.c" ^
        %LDFLAGS% %LIBS% -o "%BUILD_DIR%\wal_func_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\wal_func_test.exe.dbg" "%BUILD_DIR%\wal_func_test.exe"
    if errorlevel 1 exit /b 1
    "%BUILD_DIR%\wal_func_test.exe"
    exit /b 0

:build_wal_advanced
    echo Building WAL advanced test...
    "%GCC%" %CFLAGS% ^
        "%PPDB_DIR%\src\kvstore\wal.c" ^
        "%PPDB_DIR%\src\kvstore\wal_write.c" ^
        "%PPDB_DIR%\src\kvstore\wal_iterator.c" ^
        "%PPDB_DIR%\src\kvstore\wal_maintenance.c" ^
        "%PPDB_DIR%\src\kvstore\wal_recovery.c" ^
        "%PPDB_DIR%\src\kvstore\sync.c" ^
        "%PPDB_DIR%\src\common\logger.c" ^
        "%PPDB_DIR%\src\common\error.c" ^
        "%PPDB_DIR%\src\common\fs.c" ^
        "%PPDB_DIR%\test\white\test_framework.c" ^
        "%PPDB_DIR%\test\white\storage\test_wal_advanced.c" ^
        %LDFLAGS% %LIBS% -o "%BUILD_DIR%\wal_advanced_test.exe.dbg"
    if errorlevel 1 exit /b 1
    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\wal_advanced_test.exe.dbg" "%BUILD_DIR%\wal_advanced_test.exe"
    if errorlevel 1 exit /b 1
    "%BUILD_DIR%\wal_advanced_test.exe"
    exit /b 0

:build_ppdb_memkv
    echo Building PPDB MemKV program...
    echo Not implemented yet
    exit /b 1 

:build_rebuild
    echo.
    echo ===== 强制重新构建 =====
    echo 检查运行时文件...
    call :check_runtime_files
    if errorlevel 1 exit /b 1
    echo 清理编译文件...
    del /F /Q "%BUILD_DIR%\*.o"
    echo 恢复运行时文件...
    copy /Y "%COSMO%\crt.o" "%BUILD_DIR%\" > nul
    copy /Y "%COSMO%\ape.o" "%BUILD_DIR%\" > nul
    copy /Y "%COSMO%\ape.lds" "%BUILD_DIR%\" > nul
    copy /Y "%COSMO%\cosmopolitan.a" "%BUILD_DIR%\" > nul
    echo 清理可执行文件...
    del /F /Q "%BUILD_DIR%\*.exe"
    del /F /Q "%BUILD_DIR%\*.exe.dbg"
    echo 重新构建完成
    echo.
    exit /b 0 