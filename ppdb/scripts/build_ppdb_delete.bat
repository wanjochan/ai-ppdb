@echo off
setlocal enabledelayedexpansion

REM ���û�׼Ŀ¼
set R=%~dp0
set ROOT_DIR=%R%..
set CROSS_DIR=%ROOT_DIR%\cross9\bin
set CC=%CROSS_DIR%\x86_64-pc-linux-gnu-gcc.exe
set AR=%CROSS_DIR%\x86_64-pc-linux-gnu-ar.exe
set OBJCOPY=%CROSS_DIR%\x86_64-pc-linux-gnu-objcopy.exe

REM ���ñ����־
set COMMON_FLAGS=-O2 -Wall -Wextra -DNDEBUG -fno-pie -no-pie -mno-red-zone -fno-omit-frame-pointer -nostdlib -nostdinc
set INCLUDES=-I%ROOT_DIR% -I%ROOT_DIR%\include -I%ROOT_DIR%\src -include %ROOT_DIR%\cosmopolitan.h
set LDFLAGS=-Wl,--gc-sections -Wl,-z,max-page-size=0x1000 -fuse-ld=bfd -Wl,-T,%ROOT_DIR%\ape.lds

REM ����Դ�ļ�Ŀ¼
set SRC_DIR=%ROOT_DIR%\src
set BUILD_DIR=%ROOT_DIR%\build\release

REM ��������Ŀ¼
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
if not exist %BUILD_DIR%\include\ppdb mkdir %BUILD_DIR%\include\ppdb

REM ����Դ�ļ�
echo Compiling source files...
for %%f in (%SRC_DIR%\common\*.c %SRC_DIR%\kvstore\*.c) do (
    echo   %%f
    %CC% %COMMON_FLAGS% %INCLUDES% -c %%f -o %BUILD_DIR%\%%~nf.o
    if !errorlevel! neq 0 goto :error
)

REM ������̬��
echo Creating static library...
%AR% rcs %BUILD_DIR%\libppdb.a %BUILD_DIR%\*.o
if !errorlevel! neq 0 goto :error

REM ������̬��
echo Creating shared library...
%CC% %COMMON_FLAGS% %LDFLAGS% -shared -o %BUILD_DIR%\libppdb.so.dbg %BUILD_DIR%\*.o %ROOT_DIR%\crt.o %ROOT_DIR%\ape.o %ROOT_DIR%\cosmopolitan.a
if !errorlevel! neq 0 goto :error
%OBJCOPY% -S -O binary %BUILD_DIR%\libppdb.so.dbg %BUILD_DIR%\libppdb.so
if !errorlevel! neq 0 goto :error

REM ����ͷ�ļ�
echo Copying header files...
copy %ROOT_DIR%\include\ppdb\*.h %BUILD_DIR%\include\ppdb\
if !errorlevel! neq 0 goto :error

echo Build completed successfully
goto :eof

:error
echo Build failed with error code !errorlevel!
exit /b !errorlevel! 