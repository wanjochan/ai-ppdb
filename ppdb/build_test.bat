@echo off
setlocal enabledelayedexpansion

set COSMO=..\cosmopolitan
set CROSS9=..\cross9

:: 设置编译选项
set CFLAGS=-g -O2 -I include

:: 确保库已经构建
if not exist build\libppdb.a (
    echo Building library first...
    call build_ppdb.bat
)

:: 编译测试文件
echo Compiling test files...
for /r test %%f in (*.c) do (
    echo   Compiling %%f...
    %CROSS9%\x86_64-pc-linux-gnu-gcc %CFLAGS% -c "%%f"
)

:: 构建测试程序
echo Building test program...
%CROSS9%\x86_64-pc-linux-gnu-gcc %CFLAGS% test\test_main.c *.o -o build\ppdb_test.exe -L build -lppdb

:: 清理中间文件
echo Cleaning up...
del *.o

:: 运行测试
echo Running tests...
build\ppdb_test.exe

echo Test completed!
