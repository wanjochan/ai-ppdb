@echo off
setlocal enabledelayedexpansion

echo PPDB Lockfree Build v1.0.0 (Debug)
echo.

:: 设置编译器和工具链
set CC="%~dp0..\cross9\bin\x86_64-pc-linux-gnu-gcc.exe"
set AR="%~dp0..\cross9\bin\x86_64-pc-linux-gnu-ar.exe"
set OBJCOPY="%~dp0..\cross9\bin\x86_64-pc-linux-gnu-objcopy.exe"

:: 设置编译选项
set COMMON_FLAGS=-g -O0 -Wall -Wextra -fno-pie -no-pie -mno-red-zone -fno-omit-frame-pointer -nostdlib -nostdinc
set INCLUDES=-I"%~dp0.." -I"%~dp0..\include" -I"%~dp0..\src" -I"%~dp0..\src_lockfree" -I"%~dp0..\cosmopolitan" -include "%~dp0..\cosmopolitan\cosmopolitan.h"
set CFLAGS=%COMMON_FLAGS% %INCLUDES% -c
set LDFLAGS=-Wl,--gc-sections -Wl,-z,max-page-size=0x1000 -fuse-ld=bfd -Wl,-T,"%~dp0..\cosmopolitan\ape.lds" -nostdlib -nostdinc

:: 设置目录
set BUILD_DIR="%~dp0..\build\lockfree"
set SRC_DIR="%~dp0..\src_lockfree"
set COMMON_DIR="%~dp0..\src\common"

echo Setting up build flags...

:: 准备构建目录
echo Preparing build directory...
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

echo Building source files...

:: 编译公共文件
echo Compiling common files...
%CC% %CFLAGS% %COMMON_DIR%\logger.c -o %BUILD_DIR%\logger.o
%CC% %CFLAGS% %COMMON_DIR%\fs.c -o %BUILD_DIR%\fs.o

:: 编译无锁版本的文件
echo Compiling lockfree files...
%CC% %CFLAGS% %SRC_DIR%\kvstore\ref_count.c -o %BUILD_DIR%\ref_count.o
%CC% %CFLAGS% %SRC_DIR%\kvstore\atomic_skiplist.c -o %BUILD_DIR%\atomic_skiplist.o
%CC% %CFLAGS% %SRC_DIR%\kvstore\sharded_memtable.c -o %BUILD_DIR%\sharded_memtable.o

:: 编译测试文件
echo Building test files...
%CC% %CFLAGS% %SRC_DIR%\test\test_lockfree.c -o %BUILD_DIR%\test_lockfree.o

:: 构建测试可执行文件
echo Building test executable...
%CC% %COMMON_FLAGS% %LDFLAGS% -o %BUILD_DIR%\test_lockfree.com ^
    "%~dp0..\cosmopolitan\crt.o" ^
    "%~dp0..\cosmopolitan\ape-no-modify-self.o" ^
    %BUILD_DIR%\test_lockfree.o ^
    %BUILD_DIR%\ref_count.o ^
    %BUILD_DIR%\atomic_skiplist.o ^
    %BUILD_DIR%\sharded_memtable.o ^
    %BUILD_DIR%\logger.o ^
    %BUILD_DIR%\fs.o ^
    "%~dp0..\cosmopolitan\cosmopolitan.a"

if errorlevel 1 (
    echo Build failed
    echo Current directory: %CD%
    echo Build directory: %BUILD_DIR%
    exit /b 1
)

:: 生成 Windows 可执行文件
echo Generating Windows executable...
%OBJCOPY% -S -O binary %BUILD_DIR%\test_lockfree.com %BUILD_DIR%\test_lockfree.exe

if errorlevel 1 (
    echo Failed to generate Windows executable
    echo Current directory: %CD%
    echo Build directory: %BUILD_DIR%
    exit /b 1
)

echo Build completed successfully
echo Test executable is in %BUILD_DIR% 