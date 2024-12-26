@echo off
setlocal

REM 设置基准目录
set R=%~dp0
set ROOT_DIR=%R%..

REM 设置编译器路径
set GCC=%ROOT_DIR%\cross9\bin\x86_64-pc-linux-gnu-gcc.exe
set OBJCOPY=%ROOT_DIR%\cross9\bin\x86_64-pc-linux-gnu-objcopy.exe

REM 设置编译选项
set CFLAGS=-g -O -static -fno-pie -no-pie -mno-red-zone -fno-omit-frame-pointer -nostdlib -nostdinc -I%ROOT_DIR%
set LDFLAGS=-Wl,--gc-sections -Wl,-z,max-page-size=0x1000 -fuse-ld=bfd -Wl,-T,%ROOT_DIR%/ape.lds

REM 设置源文件
set SOURCES=%ROOT_DIR%/src/kvstore/test.c %ROOT_DIR%/src/kvstore/kvstore.c

REM 设置输出文件
set OUTPUT_DBG=%ROOT_DIR%/test.dbg
set OUTPUT=%ROOT_DIR%/test.exe

REM 编译和链接
echo Building %OUTPUT_DBG%...
%GCC% %CFLAGS% %LDFLAGS% -o %OUTPUT_DBG% %SOURCES% -include %ROOT_DIR%/cosmopolitan.h %ROOT_DIR%/crt.o %ROOT_DIR%/ape.o %ROOT_DIR%/cosmopolitan.a
if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

REM 生成最终可执行文件
echo Generating %OUTPUT%...
%OBJCOPY% -S -O binary %OUTPUT_DBG% %OUTPUT%
if errorlevel 1 (
    echo Failed to generate executable!
    exit /b 1
)

echo Build successful!
echo You can now run %OUTPUT% 