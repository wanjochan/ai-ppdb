@echo off
setlocal enabledelayedexpansion

set COSMO=..\cosmopolitan
set CROSS9=..\cross9\bin
set GCC=%CROSS9%\x86_64-pc-linux-gnu-gcc.exe
set AR=%CROSS9%\x86_64-pc-linux-gnu-ar.exe

:: 编译选项
set COMMON_FLAGS=-Wall -Wextra -fno-pie -fno-stack-protector -fno-omit-frame-pointer
set DEBUG_FLAGS=-g -O0 -DDEBUG
set RELEASE_FLAGS=-O2 -DNDEBUG
set CFLAGS=%COMMON_FLAGS% %DEBUG_FLAGS% -I ..\include -I %COSMO%

:: 链接选项
set LDFLAGS=-static -nostdlib -Wl,-T,%COSMO%\ape.lds -Wl,--gc-sections -fuse-ld=bfd
set LIBS=%COSMO%\crt.o %COSMO%\ape.o %COSMO%\cosmopolitan.a

:: 检查工具链
if not exist "%GCC%" (
    echo Error: Cross9 GCC not found at: %GCC%
    echo Please check your toolchain installation
    exit /b 1
)

:: 创建构建目录
if not exist ..\build (
    echo Creating build directory...
    mkdir ..\build
)

:: 编译源文件
echo Compiling source files...
for /r ..\src %%f in (*.c) do (
    echo   Checking %%f...
    echo %%f | findstr /i /c:"backup" > nul
    if errorlevel 1 (
        echo   Compiling %%f...
        "%GCC%" %CFLAGS% -c "%%f"
        if errorlevel 1 (
            echo Error compiling %%f
            exit /b 1
        )
    ) else (
        echo   Skipping backup file: %%f
    )
)

:: 创建静态库
echo Creating static library...
"%AR%" rcs ..\build\libppdb.a *.o
if errorlevel 1 (
    echo Error creating library
    exit /b 1
)

:: 编译主程序
echo Building main program...
"%GCC%" %CFLAGS% %LDFLAGS% ..\src\main.c -o ..\build\ppdb.exe -L ..\build -lppdb %LIBS%
if errorlevel 1 (
    echo Error building main program
    exit /b 1
)

:: 添加 APE 自修改支持
echo Adding APE self-modify support...
copy /b ..\build\ppdb.exe + %COSMO%\ape-copy-self.o ..\build\ppdb.com
if errorlevel 1 (
    echo Error adding APE support
    exit /b 1
)

:: 清理中间文件
echo Cleaning up...
del *.o

echo Build completed successfully!
echo Binary: ..\build\ppdb.com
