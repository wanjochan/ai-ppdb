@echo off
setlocal EnableDelayedExpansion

rem 设置工具链路径
set TOOLCHAIN=D:\dev\ai-ppdb\repos\cross9\bin
set GCC=%TOOLCHAIN%\x86_64-pc-linux-gnu-gcc.exe
set LD=%TOOLCHAIN%\x86_64-pc-linux-gnu-ld.exe
set OBJCOPY=%TOOLCHAIN%\x86_64-pc-linux-gnu-objcopy.exe

rem 设置编译选项
set COSMO=D:\dev\ai-ppdb\repos\cosmopolitan
set CFLAGS=-g -O0 -fno-pie -fno-pic -mno-red-zone -nostdlib -nostdinc -fno-omit-frame-pointer -D_COSMO_SOURCE -DSUPPORT_VECTOR=255 -D_HOSTLINUX=1 -D_HOSTMETAL=2 -D_HOSTWINDOWS=4 -D_HOSTXNU=8 -D_HOSTOPENBSD=16 -D_HOSTFREEBSD=32 -D_HOSTNETBSD=64 -DLINUX=1 -DXNU=8 -DOPENBSD=16 -DFREEBSD=32 -DNETBSD=64 -DWINDOWS=128 -DMETAL=256
set INCLUDES=-I%COSMO% -I%COSMO%\libc\elf -I%COSMO%\libc\runtime -I%COSMO%\libc\intrin -I%COSMO%\libc\nexgen32e -I%COSMO%\libc\str -I%COSMO%\libc\fmt -I%COSMO%\libc\log -I%COSMO%\libc\mem -I%COSMO%\libc\calls -I%COSMO%\libc\sysv -I%COSMO%\libc\proc -I%COSMO%\libc\stdio -I%COSMO%\libc\bits -I%COSMO%\libc\debug -I%COSMO%\libc\dce -I%COSMO%\libc\nt -I%COSMO%\libc\linux -I%COSMO%\libc\mac -I%COSMO%\libc\freebsd -I%COSMO%\libc\netbsd -I%COSMO%\libc\openbsd -I%COSMO%\libc\bsd -I%COSMO%\libc\sysv -I%COSMO%\libc\posix -I%COSMO%\libc\ansi -I%COSMO%\libc\c -I%COSMO%\libc\std

echo Building test target...
echo Using GCC: %GCC%
echo Using LD: %LD%
echo Using OBJCOPY: %OBJCOPY%
echo Using COSMO: %COSMO%

rem 编译测试目标
echo Compiling test_target.c...
%GCC% %CFLAGS% %INCLUDES% -c test_target.c -o test_target.o
if errorlevel 1 goto error

echo Linking test_target.com...
%LD% -T %COSMO%\ape.lds --gc-sections --build-id=none -z max-page-size=4096 --omagic test_target.o %COSMO%\ape.o %COSMO%\crt.o %COSMO%\cosmopolitan.a -o test_target.com
if errorlevel 1 goto error

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