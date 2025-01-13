@echo off
setlocal

REM 设置工具链路径（使用相对路径）
set CROSS9=..\..\..\repos\cross9
set COSMO=..\..\..\repos\cosmopolitan
set PATH=%CROSS9%\bin;%PATH%

REM 验证头文件和库文件存在
if not exist "%COSMO%\cosmopolitan.h" (
    echo Error: cosmopolitan.h not found in %COSMO%
    exit /b 1
)

REM 设置编译器和选项
set CC=x86_64-pc-linux-gnu-gcc
set CFLAGS=-g -fPIC -fvisibility=hidden -nostdinc -nostdlib -fno-pie -fno-stack-protector -mno-red-zone -O2 -I%COSMO%
set LDFLAGS=-nostdlib -nostartfiles -static -T dll.lds -Wl,--version-script=exports.txt

REM 编译动态库
echo Building test4.dl...
%CC% %CFLAGS% -c test4.c -o test4.o
%CC% %LDFLAGS% test4.o -o test4.dl

REM 编译测试程序
echo Building test4_main...
%CC% %CFLAGS% -c test4_main.c -o test4_main.o
%CC% -nostdlib -nostartfiles -static test4_main.o %COSMO%\ape.o -o test4_main.exe

echo Build complete.
endlocal 