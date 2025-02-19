@echo off
setlocal EnableDelayedExpansion

rem 清理旧文件
if exist test11.o del test11.o
if exist test11.com.dbg del test11.com.dbg
if exist test11.dl del test11.dl

rem 设置工具链路径
set TOOLCHAIN=..\..\..\repos\cross9\bin
set GCC=%TOOLCHAIN%\x86_64-pc-linux-gnu-gcc.exe
set OBJCOPY=%TOOLCHAIN%\x86_64-pc-linux-gnu-objcopy.exe
set OBJDUMP=%TOOLCHAIN%\x86_64-pc-linux-gnu-objdump.exe

rem 编译插件
echo Building test11 plugin...
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
    test11.c -o test11.o

if errorlevel 1 (
    echo Compilation failed
    exit /b 1
)

rem 检查目标文件
%OBJDUMP% -h test11.o | findstr ".text.dl"
if errorlevel 1 (
    echo Warning: Plugin sections not found
)

rem 链接插件
echo Linking plugin...
%GCC% ^
    -static -nostdlib ^
    -Wl,-T,test11.lds ^
    -Wl,--gc-sections ^
    -Wl,--build-id=none ^
    -Wl,-z,max-page-size=4096 ^
    -Wl,--wrap=main ^
    -Wl,--wrap=_init ^
    -Wl,--wrap=ape_stack_round ^
    -o test11.com.dbg ^
    test11.o ^
    ..\..\..\repos\cosmopolitan\crt.o ^
    ..\..\..\repos\cosmopolitan\cosmopolitan.a

if errorlevel 1 (
    echo Linking failed
    exit /b 1
)

rem 检查链接文件
%OBJDUMP% -h test11.com.dbg | findstr ".header"
if errorlevel 1 (
    echo Warning: Header section not found
)

rem 生成最终插件
echo Generating final plugin...
%OBJCOPY% -S -O binary test11.com.dbg test11.dl

if errorlevel 1 (
    echo Binary generation failed
    exit /b 1
)

rem 验证文件大小
for %%F in (test11.dl) do set size=%%~zF
echo Plugin size: !size! bytes

if !size! LSS 100 (
    echo Warning: Plugin file seems too small
) else (
    echo Plugin generated successfully
)

endlocal 