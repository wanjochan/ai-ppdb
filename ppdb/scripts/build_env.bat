@echo off
setlocal EnableDelayedExpansion

REM 设置目录路径
set SCRIPT_DIR=%~dp0
set ROOT_DIR=%SCRIPT_DIR%\..\..
pushd "%ROOT_DIR%"
@rem set "ROOT_DIR=%CD%"
popd
echo ROOT_DIR=%ROOT_DIR%
set PPDB_DIR=%ROOT_DIR%\ppdb
set BUILD_DIR=%PPDB_DIR%\build
set BIN_DIR=%PPDB_DIR%\bin
set SRC_DIR=%PPDB_DIR%\src
set INCLUDE_DIR=%PPDB_DIR%\include
set INTERNAL_DIR=%PPDB_DIR%\internal
set TEST_DIR=%PPDB_DIR%\test
set COSMO=%ROOT_DIR%\repos\cosmopolitan_pub
set COSMOSRC=%ROOT_DIR%\repos\cosmopolitan

REM 设置工具链路径
set CROSS9=%ROOT_DIR%\repos\cross9\bin
set GCC=%CROSS9%\x86_64-pc-linux-gnu-gcc.exe
set AR=%CROSS9%\x86_64-pc-linux-gnu-ar.exe
set OBJCOPY=%CROSS9%\x86_64-pc-linux-gnu-objcopy.exe

REM 验证工具链
if not exist "%GCC%" (
    echo Error: GCC not found at %GCC%
    exit /b 1
)

if not exist "%AR%" (
    echo Error: AR not found at %AR%
    exit /b 1
)

if not exist "%OBJCOPY%" (
    echo Error: OBJCOPY not found at %OBJCOPY%
    exit /b 1
)

REM 设置构建标志
set CFLAGS=-g -O2 -fno-pie -fno-pic -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -mcmodel=large
set LDFLAGS=-static -nostdlib -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,max-page-size=4096

REM 验证运行时文件
if not exist "%COSMO%\crt.o" (
    echo Error: crt.o not found at %COSMO%\crt.o
    exit /b 1
)

if not exist "%COSMO%\ape.o" (
    echo Error: ape.o not found at %COSMO%\ape.o
    exit /b 1
)

if not exist "%COSMO%\cosmopolitan.a" (
    echo Error: cosmopolitan.a not found at %COSMO%\cosmopolitan.a
    exit /b 1
)

if not exist "%COSMO%\ape.lds" (
    echo Error: ape.lds not found at %COSMO%\ape.lds
    exit /b 1
)

REM 导出环境变量
endlocal & (
    set "ROOT_DIR=%ROOT_DIR%"
    set "PPDB_DIR=%PPDB_DIR%"
    set "BUILD_DIR=%BUILD_DIR%"
    set "CROSS9=%CROSS9%"
    set "GCC=%GCC%"
    set "AR=%AR%"
    set "OBJCOPY=%OBJCOPY%"
    set "COSMO=%COSMO%"
    set "COSMOSRC=%COSMOSRC%"
    set "CFLAGS=%CFLAGS%"
    set "LDFLAGS=%LDFLAGS%"
    set "TEST_DIR=%TEST_DIR%"
    set "SRC_DIR=%SRC_DIR%"
)
