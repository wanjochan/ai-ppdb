@echo off
setlocal enabledelayedexpansion

REM 设置编译器和标志
set CC=gcc
set AR=ar
set CFLAGS=-g -O2 -fno-pie -no-pie -static -mno-red-zone -nostdinc -fno-omit-frame-pointer -pg
set CFLAGS=%CFLAGS% -Wno-unknown-pragmas -Wno-pragmas
set CFLAGS=%CFLAGS% -I./third_party/cosmopolitan -I./src_lockfree
set LDFLAGS=-static -no-pie -nostdlib
set LIBS=./third_party/cosmopolitan/crt.o ./third_party/cosmopolitan/ape.o ./third_party/cosmopolitan/cosmopolitan.a

REM 创建构建目录
if not exist build_lockfree mkdir build_lockfree
if not exist build_lockfree\obj mkdir build_lockfree\obj

REM 编译源文件
for %%f in (src_lockfree\kvstore\*.c) do (
    echo Compiling %%f...
    %CC% %CFLAGS% -c %%f -o build_lockfree\obj\%%~nf.o
    if errorlevel 1 (
        echo Error compiling %%f
        exit /b 1
    )
)

REM 创建静态库
echo Creating static library...
%AR% rcs build_lockfree\libppdb_lockfree.a build_lockfree\obj\*.o

REM 编译测试文件（暂时注释掉，等测试框架适配好后再启用）
REM echo Building tests...
REM for %%f in (tests\*.c) do (
REM     echo Compiling test %%f...
REM     %CC% %CFLAGS% -c %%f -o build_lockfree\obj\%%~nf.o
REM     if errorlevel 1 (
REM         echo Error compiling test %%f
REM         exit /b 1
REM     )
REM )

REM 链接测试可执行文件
REM %CC% %LDFLAGS% build_lockfree\obj\test_*.o build_lockfree\libppdb_lockfree.a %LIBS% -o build_lockfree\ppdb_lockfree_test.exe

echo Build completed successfully!
endlocal
