@echo off
setlocal EnableDelayedExpansion

REM 检查参数
if "%1"=="" (
    echo Usage: %0 ^<source_file^> [output_name]
    echo Example: %0 test.c test.dl
    echo If output_name is not specified, it will be derived from source_file
    exit /b 1
)

REM 设置源文件和输出文件名
set SOURCE=%1
set OUTPUT=%~n1.dl
if not "%2"=="" set OUTPUT=%2

REM 检查源文件是否存在
if not exist %SOURCE% (
    echo Error: Source file %SOURCE% not found
    exit /b 1
)

REM 清理旧文件
del /q %OUTPUT% %~n1.o 2>nul

REM 编译选项
set CFLAGS=-g -O2 -mcmodel=small -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -fPIC
set CFLAGS=%CFLAGS% -Wall -Wextra -Wno-unused-parameter -nostdinc
set INCLUDES=-I..\..\..\repos\cosmopolitan -I..\..\..\repos\cosmopolitan\libc -I..\..\..\repos\cosmopolitan\libc\calls
set INCLUDES=%INCLUDES% -I..\..\..\repos\cosmopolitan\libc\sock -I..\..\..\repos\cosmopolitan\libc\thread -I.. -I..\..\

REM 链接选项
set LDFLAGS=-nostdlib -shared -Wl,-T,tpl_dl.lds -Wl,-z,max-page-size=4096 -Wl,--build-id=none
set LDFLAGS=%LDFLAGS% -Wl,-z,defs -Wl,--emit-relocs -Wl,--no-undefined -Wl,--gc-sections

REM 编译
echo Compiling %SOURCE%...
..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-gcc.exe %CFLAGS% %INCLUDES% -include ..\..\..\repos\cosmopolitan\cosmopolitan.h -c %SOURCE% -o %~n1.o
if errorlevel 1 (
    echo Error: Compilation failed
    exit /b 1
)

REM 链接
echo Linking %OUTPUT%...
..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-gcc.exe %LDFLAGS% -o %OUTPUT% %~n1.o
if errorlevel 1 (
    echo Error: Linking failed
    exit /b 1
)

echo Build completed successfully: %OUTPUT%
exit /b 0 