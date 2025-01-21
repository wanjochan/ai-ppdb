@echo off
setlocal EnableDelayedExpansion

rem 清理旧文件
if exist test12.o del test12.o
if exist test12.exe del test12.exe
if exist test12.exe.dbg del test12.exe.dbg

rem 设置工具链路径
set TOOLCHAIN=..\..\..\repos\cross9\bin
set GCC=%TOOLCHAIN%\x86_64-pc-linux-gnu-gcc.exe
set OBJCOPY=%TOOLCHAIN%\x86_64-pc-linux-gnu-objcopy.exe
set OBJDUMP=%TOOLCHAIN%\x86_64-pc-linux-gnu-objdump.exe

rem 编译APE加载器
echo Building APE loader...
%GCC% -c ^
    -g -O0 -mcmodel=small ^
    -fno-omit-frame-pointer -mno-red-zone ^
    -fno-common -fno-plt ^
    -ffunction-sections -fdata-sections ^
    -Wall -Wextra -Wno-unused-parameter ^
    -nostdinc ^
    -I..\..\..\repos\cosmopolitan ^
    -I..\..\..\repos\cosmopolitan\libc ^
    -I..\..\..\repos\cosmopolitan\libc\calls ^
    -I..\..\..\repos\cosmopolitan\libc\sock ^
    -I..\..\..\repos\cosmopolitan\libc\thread ^
    -include ..\..\..\repos\cosmopolitan\cosmopolitan.h ^
    ape_loader.c -o ape_loader.o

if errorlevel 1 (
    echo APE loader compilation failed
    exit /b 1
)

rem 编译主程序
echo Building test12...
%GCC% -c ^
    -g -O0 -mcmodel=small ^
    -fno-omit-frame-pointer -mno-red-zone ^
    -fno-common -fno-plt ^
    -ffunction-sections -fdata-sections ^
    -Wall -Wextra -Wno-unused-parameter ^
    -nostdinc ^
    -I..\..\..\repos\cosmopolitan ^
    -I..\..\..\repos\cosmopolitan\libc ^
    -I..\..\..\repos\cosmopolitan\libc\calls ^
    -I..\..\..\repos\cosmopolitan\libc\sock ^
    -I..\..\..\repos\cosmopolitan\libc\thread ^
    -include ..\..\..\repos\cosmopolitan\cosmopolitan.h ^
    test12.c -o test12.o

if errorlevel 1 (
    echo Test12 compilation failed
    exit /b 1
)

rem 链接程序
echo Linking test12...
%GCC% ^
    -static -nostdlib ^
    -Wl,-T,..\..\..\repos\cosmopolitan\ape.lds ^
    -Wl,--gc-sections ^
    -Wl,--build-id=none ^
    -Wl,-z,max-page-size=4096 ^
    -Wl,--wrap=ape_stack_round ^
    -o test12.com.dbg ^
    test12.o ^
    ape_loader.o ^
    ..\..\..\repos\cosmopolitan\crt.o ^
    ..\..\..\repos\cosmopolitan\ape.o ^
    ..\..\..\repos\cosmopolitan\cosmopolitan.a

if errorlevel 1 (
    echo Linking failed
    exit /b 1
)

rem 生成最终可执行文件
echo Generating final executable...
%OBJCOPY% -S -O binary test12.com.dbg test12.exe

if errorlevel 1 (
    echo Binary generation failed
    exit /b 1
)

rem 验证文件大小
for %%F in (test12.exe) do set size=%%~zF
echo Executable size: !size! bytes

if !size! LSS 100 (
    echo Warning: Executable file seems too small
) else (
    echo Executable generated successfully
)

endlocal 