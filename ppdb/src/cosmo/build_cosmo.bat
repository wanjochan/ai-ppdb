@echo off
setlocal EnableDelayedExpansion

rem 清理旧文件
if exist test11_main.o del test11_main.o
if exist test11_main.com.dbg del test11_main.com.dbg
if exist test11_main.com del test11_main.com

rem 设置工具链路径
set TOOLCHAIN=..\..\..\repos\cross9\bin
set GCC=%TOOLCHAIN%\x86_64-pc-linux-gnu-gcc.exe
set OBJCOPY=%TOOLCHAIN%\x86_64-pc-linux-gnu-objcopy.exe

rem 编译主程序
echo Building cosmo main program...
%GCC% -c ^
    -g -O0 -mcmodel=small ^
    -fno-omit-frame-pointer -mno-red-zone ^
    -fno-common -fno-plt ^
    -Wall -Wextra -Wno-unused-parameter ^
    -nostdinc ^
    -I..\..\..\repos\cosmopolitan ^
    -I..\..\..\repos\cosmopolitan\libc ^
    -I..\..\..\repos\cosmopolitan\libc\calls ^
    -I..\..\..\repos\cosmopolitan\libc\sock ^
    -I..\..\..\repos\cosmopolitan\libc\thread ^
    -include ..\..\..\repos\cosmopolitan\cosmopolitan.h ^
    cosmo.c -o cosmo.o

if errorlevel 1 (
    echo Compilation failed
    exit /b 1
)

rem 链接主程序
echo Linking main program...
%GCC% ^
    -static -nostdlib ^
    -Wl,-T,..\..\..\repos\cosmopolitan\ape.lds ^
    -Wl,--gc-sections ^
    -Wl,--build-id=none ^
    -Wl,-z,max-page-size=4096 ^
    -Wl,--defsym=ape_stack_vaddr=0x700000000000 ^
    -Wl,--defsym=ape_stack_memsz=0x100000 ^
    -Wl,--defsym=ape_stack_round=0x1000 ^
    -Wl,--entry=_start ^
    -o cosmo.exe.dbg ^
    cosmo.o ^
    ..\..\..\repos\cosmopolitan\crt.o ^
    ..\..\..\repos\cosmopolitan\ape.o ^
    ..\..\..\repos\cosmopolitan\cosmopolitan.a

if errorlevel 1 (
    echo Linking failed
    exit /b 1
)

rem 生成最终可执行文件
echo Generating final executable...
%OBJCOPY% -S -O binary cosmo.exe.dbg cosmo.exe

if errorlevel 1 (
    echo Binary generation failed
    exit /b 1
)

rem 验证文件大小
for %%F in (cosmo.exe) do set size=%%~zF
echo Main program size: !size! bytes

if !size! LSS 100 (
    echo Warning: Main program file seems too small
) else (
    echo Main program built successfully
)

endlocal 