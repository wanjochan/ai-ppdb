@echo off
setlocal EnableDelayedExpansion

rem 设置工具链路径
set TOOLCHAIN=..\..\..\repos\cross9\bin
set GCC=%TOOLCHAIN%\x86_64-pc-linux-gnu-gcc.exe
set LD=%TOOLCHAIN%\x86_64-pc-linux-gnu-ld.exe
set OBJCOPY=%TOOLCHAIN%\x86_64-pc-linux-gnu-objcopy.exe
set OBJDUMP=%TOOLCHAIN%\x86_64-pc-linux-gnu-objdump.exe
set NM=%TOOLCHAIN%\x86_64-pc-linux-gnu-nm.exe

rem 设置编译选项
set COSMO=..\..\..\repos\cosmopolitan
set CFLAGS=-g -O0 -fno-pie -fno-pic -mno-red-zone -nostdlib -nostdinc -fno-omit-frame-pointer -pg -ffunction-sections -fdata-sections -fvisibility=hidden
set INCLUDES=-I%COSMO%

echo Building target program...
echo Using GCC: %GCC%
echo Using LD: %LD%
echo Using OBJCOPY: %OBJCOPY%
echo Using COSMO: %COSMO%

rem 编译和链接
echo Compiling test_target.c...
%GCC% %CFLAGS% %INCLUDES% -c test_target.c -o test_target.o
if errorlevel 1 goto error

echo Checking object file symbols...
%NM% -C test_target.o

echo Linking test_target.com...
%LD% -T %COSMO%/ape.lds --gc-sections --build-id=none -z max-page-size=4096 --omagic -Map=test_target.map %COSMO%/ape.o test_target.o %COSMO%/crt.o %COSMO%/cosmopolitan.a -o test_target.com
if errorlevel 1 goto error

echo Checking executable symbols...
%NM% -C test_target.com

echo Dumping section headers...
%OBJDUMP% -h test_target.com

echo Creating binary test_target.exe...
%OBJCOPY% -S -O binary test_target.com test_target.exe
if errorlevel 1 goto error

echo Checking file sizes...
dir test_target.o test_target.com test_target.exe

echo Build successful!
goto end

:error
echo Build failed!
exit /b 1

:end
endlocal 