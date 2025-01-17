@echo off
setlocal EnableDelayedExpansion

rem 设置工具链路径
set TOOLCHAIN=..\..\..\repos\cross9\bin
set GCC=%TOOLCHAIN%\x86_64-pc-linux-gnu-gcc.exe
set LD=%TOOLCHAIN%\x86_64-pc-linux-gnu-ld.exe
set OBJCOPY=%TOOLCHAIN%\x86_64-pc-linux-gnu-objcopy.exe

rem 设置编译选项
set COSMO=..\..\..\repos\cosmopolitan
set CFLAGS=-g -O0 -fno-pie -fno-pic -mno-red-zone -nostdlib -nostdinc -fno-omit-frame-pointer -pg
set INCLUDES=-I%COSMO%

echo Building loader program...
echo Using GCC: %GCC%
echo Using LD: %LD%
echo Using OBJCOPY: %OBJCOPY%
echo Using COSMO: %COSMO%

rem 编译和链接
echo Compiling test_loader.c...
%GCC% %CFLAGS% %INCLUDES% -c test_loader.c -o test_loader.o
if errorlevel 1 goto error

echo Compiling ape_loader.c...
%GCC% %CFLAGS% %INCLUDES% -c ape_loader.c -o ape_loader.o
if errorlevel 1 goto error

echo Linking test_loader.com...
%LD% -T %COSMO%/ape.lds --gc-sections --build-id=none -z max-page-size=4096 --omagic %COSMO%/ape.o test_loader.o ape_loader.o %COSMO%/crt.o %COSMO%/cosmopolitan.a -o test_loader.com
if errorlevel 1 goto error

echo Creating binary test_loader.exe...
%OBJCOPY% -S -O binary test_loader.com test_loader.exe
if errorlevel 1 goto error

echo Checking file sizes...
dir test_loader.o ape_loader.o test_loader.com test_loader.exe

echo Build successful!
goto end

:error
echo Build failed!
exit /b 1

:end
endlocal 