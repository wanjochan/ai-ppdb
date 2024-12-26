@echo off
setlocal enabledelayedexpansion

REM 设置基准目录
set R=%~dp0
set ROOT_DIR=%R%..
set CROSS_DIR=%ROOT_DIR%\cross9\bin
set CC=%CROSS_DIR%\x86_64-pc-linux-gnu-gcc.exe
set AR=%CROSS_DIR%\x86_64-pc-linux-gnu-ar.exe
set OBJCOPY=%CROSS_DIR%\x86_64-pc-linux-gnu-objcopy.exe

REM 设置编译标志
set COMMON_FLAGS=-g -O0 -Wall -Wextra -DPPDB_DEBUG -DPPDB_TEST -fno-pie -no-pie -mno-red-zone -fno-omit-frame-pointer -nostdlib -nostdinc -D_XOPEN_SOURCE=700

set INCLUDES=-I%ROOT_DIR% -I%ROOT_DIR%\include -I%ROOT_DIR%\src -I%ROOT_DIR%\test_white -include %ROOT_DIR%\cosmopolitan.h
set LDFLAGS=-Wl,--gc-sections -Wl,-z,max-page-size=0x1000 -fuse-ld=bfd -Wl,-T,%ROOT_DIR%\ape.lds

REM 设置源文件目录
set SRC_DIR=%ROOT_DIR%\src
set TEST_DIR=%ROOT_DIR%\test_white
set BUILD_DIR=%ROOT_DIR%\build\test

REM 创建输出目录
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

REM 编译源文件
echo Compiling source files...
for %%f in (%SRC_DIR%\common\*.c %SRC_DIR%\kvstore\*.c) do (
    echo   %%f
    %CC% %COMMON_FLAGS% %INCLUDES% -c %%f -o %BUILD_DIR%\%%~nf.o
    if !errorlevel! neq 0 goto :error
)

REM 编译测试框架
echo Compiling test framework...
%CC% %COMMON_FLAGS% %INCLUDES% -c %TEST_DIR%\test_framework.c -o %BUILD_DIR%\test_framework.o
if !errorlevel! neq 0 goto :error

REM 编译测试文件
echo Compiling test files...
for %%f in (%TEST_DIR%\test_*.c) do (
    echo   %%f
    %CC% %COMMON_FLAGS% %INCLUDES% -c %%f -o %BUILD_DIR%\%%~nf.o
    if !errorlevel! neq 0 goto :error
)

REM 链接测试可执行文件
echo Linking test executable...
%CC% %COMMON_FLAGS% %LDFLAGS% -o %BUILD_DIR%\ppdb_test.dbg %BUILD_DIR%\*.o %ROOT_DIR%\crt.o %ROOT_DIR%\ape.o %ROOT_DIR%\cosmopolitan.a
if !errorlevel! neq 0 goto :error
%OBJCOPY% -S -O binary %BUILD_DIR%\ppdb_test.dbg %BUILD_DIR%\ppdb_test.exe
if !errorlevel! neq 0 goto :error

REM 运行测试
echo Running tests...
%BUILD_DIR%\ppdb_test.exe
if !errorlevel! neq 0 goto :error

echo All tests completed successfully
goto :eof

:error
echo Build or test failed with error code !errorlevel!
exit /b !errorlevel! 