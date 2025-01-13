@echo off
setlocal

REM 设置工具链路径（使用相对路径）
set CROSS9=..\..\..\repos\cross9
set COSMO=..\..\..\repos\cosmopolitan
set PATH=%CROSS9%\bin;%PATH%

REM 验证文件存在
if not exist "%COSMO%\cosmopolitan.h" (
    echo Error: cosmopolitan.h not found in %COSMO%
    exit /b 1
)

REM 设置编译器和选项
set CC=x86_64-pc-linux-gnu-gcc
set CFLAGS=-g -fPIC -fvisibility=hidden -fno-pie -fno-stack-protector -O2 -nostdinc -I%COSMO% -mcmodel=large -fno-common
set LDFLAGS=-nostdlib -nostartfiles -shared -Wl,--version-script=exports.txt -Wl,-z,noexecstack -Wl,-z,now -Wl,-z,relro -Wl,--no-undefined -Wl,-Bsymbolic

REM 编译动态库
echo Building test4.dl...
%CC% %CFLAGS% -c test4.c -o test4.o
%CC% %LDFLAGS% test4.o -o test4.dl

REM 编译测试程序
echo Building test4_main...
%CC% %CFLAGS% -c test4_main.c -o test4_main.o
%CC% -nostdlib -nostartfiles -static test4_main.o %COSMO%\cosmopolitan.a -o test4_main.exe -e _start

echo Build complete.
endlocal 