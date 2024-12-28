@echo off
setlocal enabledelayedexpansion

echo PPDB Lock Version Test Build v1.0.0 (Debug)
echo Build time: %date% %time%
echo.

:: 设置编译器和工具链
set CC="%~dp0..\cross9\bin\x86_64-pc-linux-gnu-gcc.exe"
set AR="%~dp0..\cross9\bin\x86_64-pc-linux-gnu-ar.exe"
set OBJCOPY="%~dp0..\cross9\bin\x86_64-pc-linux-gnu-objcopy.exe"

:: 设置编译选项
set COMMON_FLAGS=-g -O0 -Wall -Wextra -fno-pie -no-pie -mno-red-zone -fno-omit-frame-pointer -nostdlib -nostdinc
set INCLUDES=-I"%~dp0.." -I"%~dp0..\include" -I"%~dp0..\src" -I"%~dp0..\cosmopolitan" -include "%~dp0..\cosmopolitan\cosmopolitan.h"
set CFLAGS=%COMMON_FLAGS% %INCLUDES% -c
set LDFLAGS=-Wl,--gc-sections -Wl,-z,max-page-size=0x1000 -fuse-ld=bfd -Wl,-T,"%~dp0..\cosmopolitan\ape.lds" -nostdlib -nostdinc

:: 设置目录
set BUILD_DIR="%~dp0..\build\lock"
set SRC_DIR="%~dp0..\src"
set COMMON_DIR="%~dp0..\src\common"
set TEST_DIR="%~dp0..\test\lock"

echo Setting up build flags...

:: 准备构建目录
echo Preparing build directory...
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

echo Building source files...

:: 编译公共文件
echo Compiling common files...
%CC% %CFLAGS% %COMMON_DIR%\logger.c -o %BUILD_DIR%\logger.o
%CC% %CFLAGS% %COMMON_DIR%\fs.c -o %BUILD_DIR%\fs.o

:: 编译有锁版本的文件
echo Compiling lock version files...
%CC% %CFLAGS% %SRC_DIR%\kvstore\skiplist.c -o %BUILD_DIR%\skiplist.o
%CC% %CFLAGS% %SRC_DIR%\kvstore\memtable.c -o %BUILD_DIR%\memtable.o
%CC% %CFLAGS% %SRC_DIR%\kvstore\wal.c -o %BUILD_DIR%\wal.o
%CC% %CFLAGS% %SRC_DIR%\kvstore\kvstore.c -o %BUILD_DIR%\kvstore.o

:: 编译测试文件
echo Building test files...
%CC% %CFLAGS% %TEST_DIR%\test_framework.c -o %BUILD_DIR%\test_framework.o
%CC% %CFLAGS% %TEST_DIR%\test_wal.c -o %BUILD_DIR%\test_wal.o
%CC% %CFLAGS% %TEST_DIR%\test_memtable.c -o %BUILD_DIR%\test_memtable.o
%CC% %CFLAGS% %TEST_DIR%\test_kvstore.c -o %BUILD_DIR%\test_kvstore.o

:: 构建测试可执行文件
echo Building test executables...

:: WAL测试
echo Creating WAL test binary...
%CC% %COMMON_FLAGS% %LDFLAGS% -o %BUILD_DIR%\test_wal.com ^
    "%~dp0..\cosmopolitan\crt.o" ^
    "%~dp0..\cosmopolitan\ape-no-modify-self.o" ^
    %BUILD_DIR%\test_wal.o ^
    %BUILD_DIR%\test_framework.o ^
    %BUILD_DIR%\wal.o ^
    %BUILD_DIR%\logger.o ^
    %BUILD_DIR%\fs.o ^
    "%~dp0..\cosmopolitan\cosmopolitan.a"

:: MemTable测试
echo Creating MemTable test binary...
%CC% %COMMON_FLAGS% %LDFLAGS% -o %BUILD_DIR%\test_memtable.com ^
    "%~dp0..\cosmopolitan\crt.o" ^
    "%~dp0..\cosmopolitan\ape-no-modify-self.o" ^
    %BUILD_DIR%\test_memtable.o ^
    %BUILD_DIR%\test_framework.o ^
    %BUILD_DIR%\memtable.o ^
    %BUILD_DIR%\skiplist.o ^
    %BUILD_DIR%\logger.o ^
    %BUILD_DIR%\fs.o ^
    "%~dp0..\cosmopolitan\cosmopolitan.a"

:: KVStore测试
echo Creating KVStore test binary...
%CC% %COMMON_FLAGS% %LDFLAGS% -o %BUILD_DIR%\test_kvstore.com ^
    "%~dp0..\cosmopolitan\crt.o" ^
    "%~dp0..\cosmopolitan\ape-no-modify-self.o" ^
    %BUILD_DIR%\test_kvstore.o ^
    %BUILD_DIR%\test_framework.o ^
    %BUILD_DIR%\kvstore.o ^
    %BUILD_DIR%\memtable.o ^
    %BUILD_DIR%\skiplist.o ^
    %BUILD_DIR%\wal.o ^
    %BUILD_DIR%\logger.o ^
    %BUILD_DIR%\fs.o ^
    "%~dp0..\cosmopolitan\cosmopolitan.a"

:: 生成Windows可执行文件
echo Generating Windows executables...
%OBJCOPY% -S -O binary %BUILD_DIR%\test_wal.com %BUILD_DIR%\test_wal.exe
%OBJCOPY% -S -O binary %BUILD_DIR%\test_memtable.com %BUILD_DIR%\test_memtable.exe
%OBJCOPY% -S -O binary %BUILD_DIR%\test_kvstore.com %BUILD_DIR%\test_kvstore.exe

echo Build completed successfully
echo Test executables are in %BUILD_DIR%

:: 运行测试
echo Running tests...

echo Running WAL test...
cd %BUILD_DIR% && .\test_wal.exe
if errorlevel 1 (
    echo WAL test failed
    exit /b 1
)

echo Running MemTable test...
cd %BUILD_DIR% && .\test_memtable.exe
if errorlevel 1 (
    echo MemTable test failed
    exit /b 1
)

echo Running KVStore test...
cd %BUILD_DIR% && .\test_kvstore.exe
if errorlevel 1 (
    echo KVStore test failed
    exit /b 1
)

echo All tests completed successfully 