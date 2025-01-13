@echo off
setlocal

REM 设置工具链路径
set CROSS9=D:\dev\repos\cross9
set COSMO=D:\dev\repos\cosmopolitan
set PATH=%CROSS9%\bin;%PATH%

REM 设置编译器和选项
set CC=x86_64-pc-linux-gnu-gcc
set CFLAGS=-g -fPIC -fvisibility=hidden -nostdinc -nostdlib -fno-pie -fno-stack-protector -mno-red-zone -O2 -I%COSMO%
set LDFLAGS=-shared -Wl,--version-script=exports.txt -T dll.lds -L%COSMO%

REM 编译动态库
echo Building test4.dl...
%CC% %CFLAGS% -c test4.c -o test4.o
%CC% %LDFLAGS% test4.o -o test4.dl

REM 编译测试程序
echo Building test4_main...
%CC% %CFLAGS% test4_main.c -o test4_main.exe -L%COSMO% -lcosmo

echo Build complete.
endlocal 