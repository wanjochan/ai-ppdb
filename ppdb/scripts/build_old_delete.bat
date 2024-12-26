@echo off
setlocal

REM ���û�׼Ŀ¼
set R=%~dp0
set ROOT_DIR=%R%..

REM ���ñ�����·��
set GCC=%ROOT_DIR%\cross9\bin\x86_64-pc-linux-gnu-gcc.exe
set OBJCOPY=%ROOT_DIR%\cross9\bin\x86_64-pc-linux-gnu-objcopy.exe

REM ���ñ���ѡ��
set CFLAGS=-g -O -static -fno-pie -no-pie -mno-red-zone -fno-omit-frame-pointer -nostdlib -nostdinc -I%ROOT_DIR%
set LDFLAGS=-Wl,--gc-sections -Wl,-z,max-page-size=0x1000 -fuse-ld=bfd -Wl,-T,%ROOT_DIR%/ape.lds

REM ����Դ�ļ�
set SOURCES=%ROOT_DIR%/src/kvstore/test.c %ROOT_DIR%/src/kvstore/kvstore.c

REM ��������ļ�
set OUTPUT_DBG=%ROOT_DIR%/test.dbg
set OUTPUT=%ROOT_DIR%/test.exe

REM ���������
echo Building %OUTPUT_DBG%...
%GCC% %CFLAGS% %LDFLAGS% -o %OUTPUT_DBG% %SOURCES% -include %ROOT_DIR%/cosmopolitan.h %ROOT_DIR%/crt.o %ROOT_DIR%/ape.o %ROOT_DIR%/cosmopolitan.a
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

REM �������տ�ִ���ļ�
echo Generating %OUTPUT%...
%OBJCOPY% -S -O binary %OUTPUT_DBG% %OUTPUT%
if errorlevel 1 (
    echo Failed to generate executable!
    exit /b 1
)

echo Build successful!
echo You can now run %OUTPUT% 