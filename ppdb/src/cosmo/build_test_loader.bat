@echo off
set WORK_DIR=%cd%
echo WORK_DIR=%WORK_DIR%

set SCRIPT_DIR=%~dp0
echo SCRIPT_DIR=%SCRIPT_DIR%

set REPO_DIR=%SCRIPT_DIR%..\..\..\
pushd %REPO_DIR%
set REPO_DIR=%CD%
popd
echo REPO_DIR=%REPO_DIR%

setlocal EnableDelayedExpansion

set TOOLCHAIN=%REPO_DIR%\repos\cross9\bin
set GCC=%TOOLCHAIN%\x86_64-pc-linux-gnu-gcc.exe
set LD=%TOOLCHAIN%\x86_64-pc-linux-gnu-ld.exe
set OBJCOPY=%TOOLCHAIN%\x86_64-pc-linux-gnu-objcopy.exe

set COSMOPUB=%REPO_DIR%\repos\cosmopolitan_pub
set COSMO=%REPO_DIR%\repos\cosmopolitan
set CFLAGS=-g -O0 -fno-pie -fno-pic -mno-red-zone -nostdlib -nostdinc -fno-omit-frame-pointer -D_COSMO_SOURCE -DSUPPORT_VECTOR=255 -D_HOSTLINUX=1 -D_HOSTMETAL=2 -D_HOSTWINDOWS=4 -D_HOSTXNU=8 -D_HOSTOPENBSD=16 -D_HOSTFREEBSD=32 -D_HOSTNETBSD=64 -DLINUX=1 -DXNU=8 -DOPENBSD=16 -DFREEBSD=32 -DNETBSD=64 -DWINDOWS=128 -DMETAL=256
set INCLUDES=-I%COSMOPUB% -I%COSMO% -I%COSMO%\libc\elf -I%COSMO%\libc\runtime -I%COSMO%\libc\intrin -I%COSMO%\libc\nexgen32e -I%COSMO%\libc\str -I%COSMO%\libc\fmt -I%COSMO%\libc\log -I%COSMO%\libc\mem -I%COSMO%\libc\calls -I%COSMO%\libc\sysv -I%COSMO%\libc\proc -I%COSMO%\libc\stdio -I%COSMO%\libc\bits -I%COSMO%\libc\debug -I%COSMO%\libc\dce -I%COSMO%\libc\nt -I%COSMO%\libc\linux -I%COSMO%\libc\mac -I%COSMO%\libc\freebsd -I%COSMO%\libc\netbsd -I%COSMO%\libc\openbsd -I%COSMO%\libc\bsd -I%COSMO%\libc\sysv -I%COSMO%\libc\posix -I%COSMO%\libc\ansi -I%COSMO%\libc\c -I%COSMO%\libc\std

echo Building test loader...
echo Using GCC: %GCC%
echo Using LD: %LD%
echo Using OBJCOPY: %OBJCOPY%
echo Using COSMOPUB: %COSMOPUB%
echo Using COSMO: %COSMO%

del /Q test_loader.exe

rem 编译APE加载器
echo Compiling APE loader...
%GCC% %CFLAGS% %INCLUDES% -c %COSMO%\ape\loader.c -o ape_loader.o
if errorlevel 1 goto error

rem 编译test_loader
echo Compiling test_loader.c...
%GCC% %CFLAGS% %INCLUDES% -c test_loader.c -o test_loader.o
if errorlevel 1 goto error


rem 编译系统调用
echo Compiling systemcall.S...
%GCC% %CFLAGS% %INCLUDES% -c %COSMO%\ape\systemcall.S -o systemcall.o
if errorlevel 1 goto error

rem 编译启动代码
echo Compiling launch.S...
%GCC% %CFLAGS% %INCLUDES% -c %COSMO%\ape\launch.S -o launch.o
if errorlevel 1 goto error

echo Linking test_loader.exe.dbg...
%LD% -T %COSMOPUB%\ape.lds --gc-sections --build-id=none -z max-page-size=4096 --omagic ^
    %COSMOPUB%\crt.o ^
    test_loader.o systemcall.o launch.o ^
    %COSMOPUB%\ape.o %COSMOPUB%\cosmopolitan.a ^
    -o test_loader.exe.dbg
if errorlevel 1 goto error

echo Creating binary test_loader.exe...
%OBJCOPY% -S -O binary test_loader.exe.dbg test_loader.exe
if errorlevel 1 goto error

echo Checking file sizes...
dir test_loader.o test_loader.exe.dbg test_loader.exe

echo Build successful!
goto end

:error
echo Build failed!
exit /b 1

:end
endlocal 
