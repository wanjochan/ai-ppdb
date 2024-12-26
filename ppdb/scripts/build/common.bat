@echo off
setlocal

REM 公共构建配置

REM 设置基准目录
set "R=%~dp0"
set "ROOT_DIR=%R%..\..\"
set "BUILD_DIR=%ROOT_DIR%build"
set "DATA_DIR=%ROOT_DIR%data"

REM 创建必要的目录
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
if not exist "%DATA_DIR%" mkdir "%DATA_DIR%"
if not exist "%DATA_DIR%\wal" mkdir "%DATA_DIR%\wal"

REM 设置编译器路径
set "GCC=%ROOT_DIR%cross9\bin\x86_64-pc-linux-gnu-gcc.exe"
set "OBJCOPY=%ROOT_DIR%cross9\bin\x86_64-pc-linux-gnu-objcopy.exe"

REM 设置编译选项
set "CFLAGS=-g -O -static -fno-pie -no-pie -mno-red-zone -fno-omit-frame-pointer -nostdlib -nostdinc -I%ROOT_DIR%"
set "LDFLAGS=-Wl,--gc-sections -Wl,-z,max-page-size=0x1000 -fuse-ld=bfd -Wl,-T,%ROOT_DIR%ape.lds"

REM 设置基础库文件
set "BASE_LIBS=%ROOT_DIR%crt.o %ROOT_DIR%ape.o %ROOT_DIR%cosmopolitan.a"

REM 版本信息
set "VERSION=0.1.0"
set "BUILD_TIME=%date% %time%"

REM 编译函数
:compile
    set "TARGET_NAME=%~1"
    set "SOURCE_FILES=%~2"
    set "OUTPUT_DBG=%BUILD_DIR%\%TARGET_NAME%.dbg"
    set "OUTPUT=%BUILD_DIR%\%TARGET_NAME%.exe"
    
    echo Building %TARGET_NAME%...
    echo Output: %OUTPUT%
    echo Sources: %SOURCE_FILES%
    
    %GCC% %CFLAGS% %LDFLAGS% -o "%OUTPUT_DBG%" %SOURCE_FILES% -include "%ROOT_DIR%cosmopolitan.h" %BASE_LIBS%
    if errorlevel 1 (
        echo Build failed!
        exit /b 1
    )
    
    echo Generating executable...
    %OBJCOPY% -S -O binary "%OUTPUT_DBG%" "%OUTPUT%"
    if errorlevel 1 (
        echo Failed to generate executable!
        exit /b 1
    )
    
    echo Build successful!
    echo Executable: %OUTPUT%
    exit /b 0

:main
    exit /b 0
