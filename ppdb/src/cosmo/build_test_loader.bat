@echo off
setlocal EnableDelayedExpansion

rem 设置工具链路径
set TOOLCHAIN=D:\dev\ai-ppdb\repos\cross9\bin
set GCC=%TOOLCHAIN%\x86_64-pc-linux-gnu-gcc.exe
set LD=%TOOLCHAIN%\x86_64-pc-linux-gnu-ld.exe
set OBJCOPY=%TOOLCHAIN%\x86_64-pc-linux-gnu-objcopy.exe

rem 设置编译选项
set COSMO=D:\dev\ai-ppdb\repos\cosmopolitan
set CFLAGS=-g -O0 -fno-pie -fno-pic -mno-red-zone -nostdlib -nostdinc -fno-omit-frame-pointer -DSUPPORT_VECTOR=1
set INCLUDES=-I%COSMO% -I%COSMO%\libc\elf -I%COSMO%\libc\runtime -I%COSMO%\libc\intrin -I%COSMO%\libc\nexgen32e -I%COSMO%\libc\str -I%COSMO%\libc\fmt -I%COSMO%\libc\log -I%COSMO%\libc\mem -I%COSMO%\libc\calls -I%COSMO%\libc\sysv -I%COSMO%\libc\proc -I%COSMO%\libc\stdio -I%COSMO%\libc\bits -I%COSMO%\libc\debug -I%COSMO%\libc\dce -I%COSMO%\libc\nt -I%COSMO%\libc\linux -I%COSMO%\libc\mac -I%COSMO%\libc\freebsd -I%COSMO%\libc\netbsd -I%COSMO%\libc\openbsd -I%COSMO%\libc\bsd -I%COSMO%\libc\sysv -I%COSMO%\libc\posix -I%COSMO%\libc\ansi -I%COSMO%\libc\c -I%COSMO%\libc\std

echo Building APE loader...
echo Using GCC: %GCC%
echo Using LD: %LD%
echo Using OBJCOPY: %OBJCOPY%
echo Using COSMO: %COSMO%

rem 编译和链接
echo Compiling loader.c...
%GCC% %CFLAGS% %INCLUDES% -c %COSMO%\ape\loader.c -o loader.o
if errorlevel 1 goto error

echo Compiling start.S...
%GCC% %CFLAGS% %INCLUDES% -c start.S -o start.o
if errorlevel 1 goto error

echo Compiling systemcall.S...
%GCC% %CFLAGS% %INCLUDES% -c %COSMO%\ape\systemcall.S -o systemcall.o
if errorlevel 1 goto error

echo Compiling launch.S...
%GCC% %CFLAGS% %INCLUDES% -c %COSMO%\ape\launch.S -o launch.o
if errorlevel 1 goto error

echo Compiling host.S...
%GCC% %CFLAGS% %INCLUDES% -c host.S -o host.o
if errorlevel 1 goto error

echo Linking loader.com...
%LD% -T %COSMO%\ape\loader.lds --gc-sections --build-id=none -z max-page-size=4096 --omagic start.o systemcall.o launch.o host.o loader.o -o loader.com
if errorlevel 1 goto error

echo Creating binary loader.exe...
%OBJCOPY% -S -O binary loader.com loader.exe
if errorlevel 1 goto error

echo Checking file sizes...
dir loader.o loader.com loader.exe

echo Build successful!
goto end

:error
echo Build failed!
exit /b 1

:end
endlocal 