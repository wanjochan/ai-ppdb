@echo off
setlocal enabledelayedexpansion

set COSMO=..\cosmopolitan
set CROSS9=..\cross9

:: 设置编译选项
set CFLAGS=-g -O2 -I include

:: 创建构建目录
if not exist build mkdir build

:: 编译源文件
echo Compiling source files...
for /r src %%f in (*.c) do (
    echo   Compiling %%f...
    %CROSS9%\x86_64-pc-linux-gnu-gcc %CFLAGS% -c "%%f"
)

:: 创建静态库
echo Creating static library...
%CROSS9%\x86_64-pc-linux-gnu-ar rcs build\libppdb.a *.o

:: 编译主程序
echo Building main program...
%CROSS9%\x86_64-pc-linux-gnu-gcc %CFLAGS% src\main.c -o build\ppdb.exe -L build -lppdb

:: 清理中间文件
echo Cleaning up...
del *.o

echo Build completed successfully!
