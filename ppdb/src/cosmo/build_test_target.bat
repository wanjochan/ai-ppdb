@echo off
setlocal EnableDelayedExpansion

rem 清理旧文件
if exist test_target.o del test_target.o
if exist test_target.com.dbg del test_target.com.dbg
if exist test_target.com del test_target.com
if exist test_target.dl del test_target.dl

rem 设置工具链路径
set TOOLCHAIN=..\..\..\repos\cross9\bin
set GCC=%TOOLCHAIN%\x86_64-pc-linux-gnu-gcc.exe
set OBJCOPY=%TOOLCHAIN%\x86_64-pc-linux-gnu-objcopy.exe

rem 编译目标程序
echo Building plugin...
%GCC% -c ^
    -g -O0 -mcmodel=small ^
    -fno-omit-frame-pointer -mno-red-zone ^
    -fno-common -fno-plt ^
    -fPIC -fno-stack-protector ^
    -Wall -Wextra -Wno-unused-parameter ^
    -nostdinc ^
    -I..\..\..\repos\cosmopolitan ^
    -I..\..\..\repos\cosmopolitan\libc ^
    -I..\..\..\repos\cosmopolitan\libc\calls ^
    -I..\..\..\repos\cosmopolitan\libc\sock ^
    -I..\..\..\repos\cosmopolitan\libc\thread ^
    -include ..\..\..\repos\cosmopolitan\cosmopolitan.h ^
    test_target.c -o test_target.o

if errorlevel 1 (
    echo Compilation failed
    exit /b 1
)

rem 链接目标程序
echo Linking plugin...
%GCC% ^
    -nostdlib ^
    -Wl,-T,plugin.lds ^
    -Wl,--gc-sections ^
    -Wl,--build-id=none ^
    -Wl,-z,max-page-size=4096 ^
    -Wl,--emit-relocs ^
    -Wl,--no-dynamic-linker ^
    -Wl,-Map=test_target.map ^
    -Wl,--verbose ^
    -o test_target.dl ^
    test_target.o ^
    ..\..\..\repos\cosmopolitan\cosmopolitan.a

if errorlevel 1 (
    echo Linking failed
    exit /b 1
)

rem 验证文件大小
for %%F in (test_target.dl) do set size=%%~zF
echo Plugin size: !size! bytes

if !size! LSS 100 (
    echo Warning: Plugin file seems too small
) else (
    echo Plugin built successfully
)

endlocal 