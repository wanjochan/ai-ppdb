@echo off
setlocal enabledelayedexpansion

set COSMO=..\cosmopolitan
set CROSS9=..\cross9\bin
set GCC=%CROSS9%\x86_64-pc-linux-gnu-gcc.exe
set AR=%CROSS9%\x86_64-pc-linux-gnu-ar.exe

:: 编译选项
set COMMON_FLAGS=-Wall -Wextra -fno-pie -fno-stack-protector -fno-omit-frame-pointer
set DEBUG_FLAGS=-g -O0 -DDEBUG
set CFLAGS=%COMMON_FLAGS% %DEBUG_FLAGS% -I ..\include -I %COSMO%

:: 链接选项
set LDFLAGS=-static -nostdlib -Wl,-T,%COSMO%\ape.lds -Wl,--gc-sections -fuse-ld=bfd
set LIBS=%COSMO%\crt.o %COSMO%\ape.o %COSMO%\cosmopolitan.a

:: 确保库已经构建
if not exist ..\build\libppdb.a (
    echo Building library first...
    call build_ppdb.bat
    if errorlevel 1 (
        echo Error building library
        exit /b 1
    )
)

:: 编译测试文件
echo Compiling test files...

:: 编译单元测试
echo   Compiling unit tests...
for /r ..\test\unit %%f in (*.c) do (
    echo     Compiling %%f...
    "%GCC%" %CFLAGS% -c "%%f"
    if errorlevel 1 (
        echo Error compiling %%f
        exit /b 1
    )
)

:: 编译集成测试
echo   Compiling integration tests...
for /r ..\test\integration %%f in (*.c) do (
    echo     Compiling %%f...
    "%GCC%" %CFLAGS% -c "%%f"
    if errorlevel 1 (
        echo Error compiling %%f
        exit /b 1
    )
)

:: 构建测试程序
echo Building test program...
"%GCC%" %CFLAGS% %LDFLAGS% ..\test\test_main.c *.o -o ..\build\ppdb_test.exe -L ..\build -lppdb %LIBS%
if errorlevel 1 (
    echo Error building test program
    exit /b 1
)

:: 添加 APE 自修改支持
echo Adding APE self-modify support...
copy /b ..\build\ppdb_test.exe + %COSMO%\ape-copy-self.o ..\build\ppdb_test.com
if errorlevel 1 (
    echo Error adding APE support
    exit /b 1
)

:: 清理中间文件
echo Cleaning up...
del *.o

:: 运行测试
echo Running tests...
echo   Running unit tests...
..\build\ppdb_test.com --unit
if errorlevel 1 (
    echo Unit tests failed
    exit /b 1
)

echo   Running integration tests...
..\build\ppdb_test.com --integration
if errorlevel 1 (
    echo Integration tests failed
    exit /b 1
)

echo All tests passed successfully!
echo Test Binary: ..\build\ppdb_test.com
