@echo off
setlocal enabledelayedexpansion

REM ==================== 环境设置 ====================
set R=%~dp0
set ROOT_DIR=%R%..
set CROSS_DIR=%ROOT_DIR%\cross9\bin
set COSMO_DIR=%ROOT_DIR%\cosmopolitan
set BUILD_DIR=%ROOT_DIR%\build\common

REM 设置编译器
set CC=%CROSS_DIR%\x86_64-pc-linux-gnu-gcc.exe
set AR=%CROSS_DIR%\x86_64-pc-linux-gnu-ar.exe

REM 编译选项
set CFLAGS=-g -O2 -Wall -Wextra -I%ROOT_DIR%\include -I%COSMO_DIR%

REM ==================== 创建目录 ====================
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

REM ==================== 编译共用模块 ====================
echo Building common modules...

REM 编译logger
%CC% %CFLAGS% -c %ROOT_DIR%\src\common\logger.c -o %BUILD_DIR%\logger.o
if errorlevel 1 goto error

REM 编译fs
%CC% %CFLAGS% -c %ROOT_DIR%\src\common\fs.c -o %BUILD_DIR%\fs.o
if errorlevel 1 goto error

REM 创建静态库
%AR% rcs %BUILD_DIR%\libppdb_common.a %BUILD_DIR%\logger.o %BUILD_DIR%\fs.o
if errorlevel 1 goto error

echo Common modules built successfully!
exit /b 0

:error
echo Build failed!
exit /b 1 