@echo off
setlocal enabledelayedexpansion

echo PPDB Test Build v1.0.0 (Debug)
echo.

:: 设置编译器和工具链
set CC="%~dp0..\cross9\bin\x86_64-pc-linux-gnu-gcc.exe"
set AR="%~dp0..\cross9\bin\x86_64-pc-linux-gnu-ar.exe"
set OBJCOPY="%~dp0..\cross9\bin\x86_64-pc-linux-gnu-objcopy.exe"

:: 设置编译选项
set COMMON_FLAGS=-g -O0 -Wall -Wextra -fno-pie -no-pie -mno-red-zone -fno-omit-frame-pointer -nostdlib -nostdinc
set INCLUDES=-I"%~dp0.." -I"%~dp0..\include" -I"%~dp0..\src" -I"%~dp0..\test_white" -I"%~dp0..\cosmopolitan" -include "%~dp0..\cosmopolitan\cosmopolitan.h"
set CFLAGS=%COMMON_FLAGS% %INCLUDES% -c
set LDFLAGS=-Wl,--gc-sections -Wl,-z,max-page-size=0x1000 -fuse-ld=bfd -Wl,-T,"%~dp0..\cosmopolitan\ape.lds" -nostdlib -nostdinc -Wl,--subsystem,console -Wl,--image-base,0x140000000

:: 设置目录
set BUILD_DIR="%~dp0..\build\test"
set SRC_DIR="%~dp0..\src"
set TEST_DIR="%~dp0..\test_white"

echo Setting up build flags...

:: 准备构建目录
echo Preparing build directory...
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

echo Building source files...

:: 编译公共文件
echo Compiling common files...
%CC% %CFLAGS% %SRC_DIR%\common\logger.c -o %BUILD_DIR%\logger.o
%CC% %CFLAGS% %SRC_DIR%\common\fs.c -o %BUILD_DIR%\fs.o

:: 编译 kvstore 文件
echo Compiling kvstore files...
%CC% %CFLAGS% %SRC_DIR%\kvstore\memtable.c -o %BUILD_DIR%\memtable.o
%CC% %CFLAGS% %SRC_DIR%\kvstore\skiplist.c -o %BUILD_DIR%\skiplist.o
%CC% %CFLAGS% %SRC_DIR%\kvstore\wal.c -o %BUILD_DIR%\wal.o
%CC% %CFLAGS% %SRC_DIR%\kvstore\kvstore.c -o %BUILD_DIR%\kvstore.o

:: 编译测试框架
echo Building test files...
echo Compiling test framework...
%CC% %CFLAGS% %TEST_DIR%\test_framework.c -o %BUILD_DIR%\test_framework.o

:: 编译测试文件
echo Compiling test files...
%CC% %CFLAGS% %TEST_DIR%\test_kvstore.c -o %BUILD_DIR%\test_kvstore.o
%CC% %CFLAGS% %TEST_DIR%\test_memtable.c -o %BUILD_DIR%\test_memtable.o
%CC% %CFLAGS% %TEST_DIR%\test_wal.c -o %BUILD_DIR%\test_wal.o

%CC% %CFLAGS% %TEST_DIR%\test_kvstore_main.c -o %BUILD_DIR%\test_kvstore_main.o
%CC% %CFLAGS% %TEST_DIR%\test_memtable_main.c -o %BUILD_DIR%\test_memtable_main.o
%CC% %CFLAGS% %TEST_DIR%\test_wal_main.c -o %BUILD_DIR%\test_wal_main.o

:: 构建测试可执行文件
echo Building test executables...

:: 构建 KVStore 测试
echo Building KVStore test...
%CC% %COMMON_FLAGS% %LDFLAGS% -o %BUILD_DIR%\test_kvstore.exe ^
    "%~dp0..\cosmopolitan\crt.o" ^
    "%~dp0..\cosmopolitan\ape-no-modify-self.o" ^
    %BUILD_DIR%\test_framework.o ^
    %BUILD_DIR%\test_kvstore.o ^
    %BUILD_DIR%\test_kvstore_main.o ^
    %BUILD_DIR%\fs.o ^
    %BUILD_DIR%\logger.o ^
    %BUILD_DIR%\kvstore.o ^
    %BUILD_DIR%\memtable.o ^
    %BUILD_DIR%\skiplist.o ^
    %BUILD_DIR%\wal.o ^
    "%~dp0..\cosmopolitan\cosmopolitan.a"

:: 构建 Memtable 测试
echo Building Memtable test...
%CC% %COMMON_FLAGS% %LDFLAGS% -o %BUILD_DIR%\test_memtable.exe ^
    "%~dp0..\cosmopolitan\crt.o" ^
    "%~dp0..\cosmopolitan\ape-no-modify-self.o" ^
    %BUILD_DIR%\test_framework.o ^
    %BUILD_DIR%\test_memtable.o ^
    %BUILD_DIR%\test_memtable_main.o ^
    %BUILD_DIR%\fs.o ^
    %BUILD_DIR%\logger.o ^
    %BUILD_DIR%\memtable.o ^
    %BUILD_DIR%\skiplist.o ^
    "%~dp0..\cosmopolitan\cosmopolitan.a"

:: 构建 WAL 测试
echo Building WAL test...
%CC% %COMMON_FLAGS% %LDFLAGS% -o %BUILD_DIR%\test_wal.exe ^
    "%~dp0..\cosmopolitan\crt.o" ^
    "%~dp0..\cosmopolitan\ape-no-modify-self.o" ^
    %BUILD_DIR%\test_framework.o ^
    %BUILD_DIR%\test_wal.o ^
    %BUILD_DIR%\test_wal_main.o ^
    %BUILD_DIR%\fs.o ^
    %BUILD_DIR%\logger.o ^
    %BUILD_DIR%\wal.o ^
    %BUILD_DIR%\memtable.o ^
    %BUILD_DIR%\skiplist.o ^
    "%~dp0..\cosmopolitan\cosmopolitan.a"

if errorlevel 1 (
    echo Build failed
    echo Current directory: %CD%
    echo Build directory: %BUILD_DIR%
    exit /b 1
)

echo Build completed successfully
echo Test executables are in %BUILD_DIR%
