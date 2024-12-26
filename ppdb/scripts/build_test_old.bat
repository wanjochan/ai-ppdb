@echo off
setlocal enabledelayedexpansion

REM ==================== 版本信息 ====================
set VERSION=1.0.0
set BUILD_TYPE=Debug
set BUILD_TIME=%date% %time%

REM ==================== 命令行参数处理 ====================
set CLEAN_ONLY=0
set BUILD_ONLY=0
set SHOW_HELP=0

:arg_loop
if "%1"=="" goto arg_done
if /i "%1"=="--clean" set CLEAN_ONLY=1
if /i "%1"=="--build-only" set BUILD_ONLY=1
if /i "%1"=="--help" set SHOW_HELP=1
shift
goto arg_loop
:arg_done

if %SHOW_HELP%==1 (
    echo Usage: %~nx0 [options]
    echo Options:
    echo   --clean      Clean build directory only
    echo   --build-only Build without running tests
    echo   --help       Show this help message
    exit /b 0
)

REM ==================== 环境检查 ====================
echo PPDB Test Build v%VERSION% (%BUILD_TYPE%)
echo Build time: %BUILD_TIME%
echo.
echo Checking environment...

REM 设置基准目录
set R=%~dp0
set ROOT_DIR=%R%..
set CROSS_DIR=%ROOT_DIR%\cross9\bin
set COSMOPOLITAN_DIR=%ROOT_DIR%\cosmopolitan
set CC=%CROSS_DIR%\x86_64-pc-linux-gnu-gcc.exe
set AR=%CROSS_DIR%\x86_64-pc-linux-gnu-ar.exe
set OBJCOPY=%CROSS_DIR%\x86_64-pc-linux-gnu-objcopy.exe

REM ==================== 设置编译选项 ====================
echo Setting up build flags...

REM 设置编译标志
set COMMON_FLAGS=-g -O0 -Wall -Wextra -DPPDB_DEBUG -DPPDB_TEST -DPPDB_VERSION=\"%VERSION%\" -fno-pie -no-pie -mno-red-zone -fno-omit-frame-pointer -nostdlib -nostdinc -D_XOPEN_SOURCE=700
set INCLUDES=-I"%ROOT_DIR%" -I"%ROOT_DIR%\include" -I"%ROOT_DIR%\src" -I"%ROOT_DIR%\test_white" -I"%COSMOPOLITAN_DIR%"
set LDFLAGS=-Wl,--gc-sections -Wl,-z,max-page-size=0x1000 -fuse-ld=bfd -Wl,-T,"%COSMOPOLITAN_DIR%\ape.lds"

REM 设置源文件目录
set SRC_DIR=%ROOT_DIR%\src
set TEST_DIR=%ROOT_DIR%\test_white
set BUILD_DIR=%ROOT_DIR%\build\test

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

REM ==================== 编译测试文件 ====================
echo Building test files...

echo Compiling test framework...
%CC% %COMMON_FLAGS% %INCLUDES% -c "%TEST_DIR%\test_framework.c" -o "%BUILD_DIR%\test_framework.o"
if !errorlevel! neq 0 goto :error

echo Compiling test files...
for %%f in ("%TEST_DIR%\test_*.c") do (
    if not "%%~nxf"=="test_framework.c" (
        echo   %%~nxf
        %CC% %COMMON_FLAGS% %INCLUDES% -c "%%f" -o "%BUILD_DIR%\%%~nf.o"
        if !errorlevel! neq 0 goto :error
    )
)

REM ==================== 链接 ====================
echo Linking...

echo Creating test executable...
%CC% %COMMON_FLAGS% %LDFLAGS% -o "%BUILD_DIR%\ppdb_test.dbg" "%BUILD_DIR%\*.o" "%COSMOPOLITAN_DIR%\crt.o" "%COSMOPOLITAN_DIR%\ape-no-modify-self.o" "%COSMOPOLITAN_DIR%\cosmopolitan.a"
if !errorlevel! neq 0 goto :error

echo Creating final binary...
%OBJCOPY% -S -O binary "%BUILD_DIR%\ppdb_test.dbg" "%BUILD_DIR%\ppdb_test.exe"
if !errorlevel! neq 0 goto :error

if %BUILD_ONLY%==1 (
    echo Build completed successfully
    goto :eof
)

REM ==================== 运行测试 ====================
echo Running tests...
"%BUILD_DIR%\ppdb_test.exe"
if !errorlevel! neq 0 goto :error

echo All tests completed successfully
goto :eof

:error
echo.
echo Build failed with error code !errorlevel!
echo Current directory: %CD%
echo Build directory: %BUILD_DIR%
exit /b !errorlevel! 
