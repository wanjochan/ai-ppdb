@echo off
setlocal EnableDelayedExpansion

rem 清理旧文件
if exist test_ape_call.o del test_ape_call.o
if exist plugin.o del plugin.o
if exist test_ape_call.com.dbg del test_ape_call.com.dbg
if exist test_ape_call.com del test_ape_call.com

rem 设置工具链路径
set TOOLCHAIN=..\..\..\repos\cross9\bin
set GCC=%TOOLCHAIN%\x86_64-pc-linux-gnu-gcc.exe
set OBJCOPY=%TOOLCHAIN%\x86_64-pc-linux-gnu-objcopy.exe

rem 编译插件加载器
echo Building plugin loader...
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
    plugin.c -o plugin.o

if errorlevel 1 (
    echo Plugin loader compilation failed
    exit /b 1
)

rem 编译主程序
echo Building loader main...
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
    test_ape_call.c -o test_ape_call.o

if errorlevel 1 (
    echo Main program compilation failed
    exit /b 1
)

rem 链接程序
echo Linking loader...
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
    -o test_ape_call.com.dbg ^
    test_ape_call.o plugin.o ^
    ..\..\..\repos\cosmopolitan\crt.o ^
    ..\..\..\repos\cosmopolitan\ape.o ^
    ..\..\..\repos\cosmopolitan\cosmopolitan.a

if errorlevel 1 (
    echo Linking failed
    exit /b 1
)

rem 生成最终可执行文件
echo Generating final executable...
%OBJCOPY% -S -O binary test_ape_call.com.dbg test_ape_call.com

if errorlevel 1 (
    echo Binary generation failed
    exit /b 1
)

rem 验证文件大小
for %%F in (test_ape_call.com) do set size=%%~zF
echo Loader size: !size! bytes

if !size! LSS 100 (
    echo Warning: Loader file seems too small
) else (
    echo Loader built successfully
)

endlocal 