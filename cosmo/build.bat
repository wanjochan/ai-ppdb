@echo off
@rem dont't edit the first 4 lines
chcp 65001 > nul
setlocal EnableDelayedExpansion

if "%1"=="" (
    echo Usage: %0 ^<module^>
    exit /b 1
)

set MODULE=%1
set CROSS=..\cross9\bin\x86_64-pc-linux-gnu-
set CFLAGS=-Wall -Wextra -O2 -g -fPIC -fno-stack-protector -fno-asynchronous-unwind-tables -nostdinc -I.
set LDFLAGS=-nostdlib -T dll.lds

if not exist build mkdir build

:: 编译模块
%CROSS%gcc.exe %CFLAGS% -c %MODULE%.c -o build\%MODULE%.o
if errorlevel 1 exit /b 1

:: 链接成 ELF
%CROSS%ld.exe %LDFLAGS% -o build\%MODULE%.elf build\%MODULE%.o
if errorlevel 1 exit /b 1

:: 复制结果文件
copy /y build\%MODULE%.elf %MODULE%.elf > nul

:: 如果是 test3，则使用 Cosmopolitan
if "%MODULE%"=="test3" (
    :: 使用 cosmocc 重新编译
    cosmocc -o test3.com test3.c
    if errorlevel 1 exit /b 1
    
    :: 生成 APE 格式文件
    objcopy -S -O binary test3.com test3.ape
    if errorlevel 1 exit /b 1
)
