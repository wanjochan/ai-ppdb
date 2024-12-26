@echo off
setlocal enabledelayedexpansion

REM ==================== 版本信息 ====================
set VERSION=1.0.0
set BUILD_TYPE=Release
set BUILD_TIME=%date% %time%

REM ==================== 命令行参数处理 ====================
set CLEAN_ONLY=0
set SHOW_HELP=0

:arg_loop
if "%1"=="" goto arg_done
if /i "%1"=="--clean" set CLEAN_ONLY=1
if /i "%1"=="--help" set SHOW_HELP=1
shift
goto arg_loop
:arg_done

if %SHOW_HELP%==1 (
    echo Usage: %~nx0 [options]
    echo Options:
    echo   --clean      Clean build directory only
    echo   --help       Show this help message
    exit /b 0
)

REM ==================== 环境检查 ====================
echo PPDB Library Build v%VERSION% (%BUILD_TYPE%)
echo Build time: %BUILD_TIME%
echo.
echo Checking environment...

REM 设置基准目录
set R=%~dp0
set ROOT_DIR=%R%..
set CROSS_DIR=%ROOT_DIR%\cross9\bin
set COSMO_DIR=%ROOT_DIR%\cosmopolitan
set CC=%CROSS_DIR%\x86_64-pc-linux-gnu-gcc.exe
set AR=%CROSS_DIR%\x86_64-pc-linux-gnu-ar.exe
set OBJCOPY=%CROSS_DIR%\x86_64-pc-linux-gnu-objcopy.exe

REM 检查必要工具
if not exist "%CC%" (
    echo Error: Compiler not found at %CC%
    goto :error
)
if not exist "%AR%" (
    echo Error: AR not found at %AR%
    goto :error
)
if not exist "%OBJCOPY%" (
    echo Error: OBJCOPY not found at %OBJCOPY%
    goto :error
)
REM 检查必要文件
if not exist "%COSMO_DIR%\cosmopolitan.h" (
    echo Error: cosmopolitan.h not found
    goto :error
)
if not exist "%COSMO_DIR%\ape.lds" (
    echo Error: ape.lds not found
    goto :error
)
if not exist "%COSMO_DIR%\crt.o" (
    echo Error: crt.o not found
    goto :error
)
if not exist "%COSMO_DIR%\ape-no-modify-self.o" (
    echo Error: ape-no-modify-self.o not found
    goto :error
)
if not exist "%COSMO_DIR%\cosmopolitan.a" (
    echo Error: cosmopolitan.a not found
    goto :error
)
REM ==================== 设置编译选项 ====================
echo Setting up build flags...

REM 设置编译标志
set COMMON_FLAGS=-O2 -Wall -Wextra -DNDEBUG -DPPDB_VERSION=\"%VERSION%\" -fno-pie -no-pie -mno-red-zone -fno-omit-frame-pointer -nostdlib -nostdinc
set INCLUDES=-I"%ROOT_DIR%" -I"%ROOT_DIR%\include" -I"%ROOT_DIR%\src" -include "%COSMO_DIR%\cosmopolitan.h"
set LDFLAGS=-Wl,--gc-sections -Wl,-z,max-page-size=0x1000 -fuse-ld=bfd -Wl,-T,"%COSMO_DIR%\ape.lds"

REM 设置源文件目录
set SRC_DIR=%ROOT_DIR%\src
set BUILD_DIR=%ROOT_DIR%\build\release

REM ==================== 准备构建 ====================
echo Preparing build directory...

REM 清理并创建输出目录
if exist "%BUILD_DIR%" (
    echo Cleaning build directory...
    del /q "%BUILD_DIR%\*.*" 2>nul
)
if %CLEAN_ONLY%==1 (
    echo Clean completed
    exit /b 0
)
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%BUILD_DIR%\include\ppdb" mkdir "%BUILD_DIR%\include\ppdb"

REM ==================== 编译源文件 ====================
echo Building source files...

echo Compiling common files...
for %%f in ("%SRC_DIR%\common\*.c") do (
    echo   %%~nxf
    %CC% %COMMON_FLAGS% %INCLUDES% -c "%%f" -o "%BUILD_DIR%\common_%%~nf.o"
    if !errorlevel! neq 0 goto :error
)

echo Compiling kvstore files...
for %%f in ("%SRC_DIR%\kvstore\*.c") do (
    echo   %%~nxf
    %CC% %COMMON_FLAGS% %INCLUDES% -c "%%f" -o "%BUILD_DIR%\kvstore_%%~nf.o"
    if !errorlevel! neq 0 goto :error
)

echo Compiling main files...
for %%f in ("%SRC_DIR%\*.c") do (
    echo   %%~nxf
    %CC% %COMMON_FLAGS% %INCLUDES% -c "%%f" -o "%BUILD_DIR%\%%~nf.o"
    if !errorlevel! neq 0 goto :error
)

REM ==================== 创建库文件 ====================
echo Creating libraries...

echo Creating static library...
%AR% rcs "%BUILD_DIR%\libppdb.a" "%BUILD_DIR%\*.o"
if !errorlevel! neq 0 goto :error

echo Creating shared library...
%CC% %COMMON_FLAGS% %LDFLAGS% -shared -o "%BUILD_DIR%\libppdb.so.dbg" "%BUILD_DIR%\*.o" "%COSMO_DIR%\crt.o" "%COSMO_DIR%\ape-no-modify-self.o" "%COSMO_DIR%\cosmopolitan.a"
if !errorlevel! neq 0 goto :error

echo Creating final binary...
%OBJCOPY% -S -O binary "%BUILD_DIR%\libppdb.so.dbg" "%BUILD_DIR%\libppdb.so"
if !errorlevel! neq 0 goto :error

REM ==================== 复制头文件 ====================
echo Copying header files...
copy "%ROOT_DIR%\include\ppdb\*.h" "%BUILD_DIR%\include\ppdb\"
if !errorlevel! neq 0 goto :error

echo Build completed successfully
goto :eof

:error
echo.
echo Build failed with error code !errorlevel!
echo Current directory: %CD%
echo Build directory: %BUILD_DIR%
exit /b !errorlevel! 
