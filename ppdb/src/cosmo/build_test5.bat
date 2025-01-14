@echo off
setlocal EnableDelayedExpansion

REM 清理旧文件
del /q test5.o test5.dl cosmo.o cosmo.exe 2>nul

REM 加载环境变量和通用函数
call "..\..\..\ppdb\scripts\build_env.bat"
if errorlevel 1 exit /b 1

REM 设置工具链路径
set CROSS9=..\..\..\repos\cross9\bin
set GCC=%CROSS9%\x86_64-pc-linux-gnu-gcc.exe
set AR=%CROSS9%\x86_64-pc-linux-gnu-ar.exe
set OBJCOPY=%CROSS9%\x86_64-pc-linux-gnu-objcopy.exe

REM 验证工具链
if not exist "%OBJCOPY%" (
    echo Error: OBJCOPY not found at %OBJCOPY%
    exit /b 1
)

REM ========== 编译选项设置 ==========
REM 通用选项
set COMMON_FLAGS=-g -O2 -mcmodel=small -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -fvisibility=hidden
set COMMON_WARNS=-Wall -Wextra -Wno-unused-parameter
set INCLUDE_FLAGS=-nostdinc -I%COSMO% -I%COSMO%\libc -I%COSMO%\libc\calls -I%COSMO%\libc\sock -I%COSMO%\libc\thread

REM DL 特定选项
set CFLAGS_DL=%COMMON_FLAGS% %COMMON_WARNS% %INCLUDE_FLAGS% -include %COSMO%\cosmopolitan.h -fPIC -shared -DSHARED_LIBRARY

REM EXE 特定选项
set CFLAGS_EXE=%COMMON_FLAGS% %COMMON_WARNS% %INCLUDE_FLAGS% -include %COSMO%\cosmopolitan.h

REM ========== 构建 DL ==========
echo Building DL...

REM 编译 DL 源文件
echo Compiling test5.c...
"%GCC%" %CFLAGS_DL% -c test5.c -o test5.o
if errorlevel 1 goto error

REM 链接 DL
echo Linking test5.dl...
"%GCC%" -static -nostdlib -Wl,-T,dl5.lds -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,max-page-size=4096 -Wl,--no-relax -Wl,--export-dynamic -Wl,-E -Wl,--emit-relocs -Wl,--defsym=ape_stack_vaddr=0x700000000000 -Wl,--defsym=ape_stack_memsz=0x100000 -Wl,--defsym=ape_stack_round=0x1000 -o test5.dl test5.o %BUILD_DIR%\crt.o %BUILD_DIR%\ape.o %BUILD_DIR%\cosmopolitan.a
if errorlevel 1 goto error

REM 显示 DL 大小
echo DL size:
dir test5.dl | findstr "test5.dl"

REM ========== 构建 EXE ==========
echo Building loader program...

REM 编译主程序
echo Compiling cosmo.c...
"%GCC%" %CFLAGS_EXE% -c cosmo.c -o cosmo.o
if errorlevel 1 goto error

REM 链接可执行文件
echo Linking cosmo.exe...
"%GCC%" -static -nostdlib -Wl,-T,%BUILD_DIR%\ape.lds -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,max-page-size=4096 -Wl,--defsym=ape_stack_vaddr=0x700000000000 -Wl,--defsym=ape_stack_memsz=0x100000 -Wl,--defsym=ape_stack_round=0x1000 -o cosmo.exe.dbg cosmo.o %BUILD_DIR%\crt.o %BUILD_DIR%\ape.o %BUILD_DIR%\cosmopolitan.a
if errorlevel 1 goto error

REM 生成最终可执行文件
echo Creating binary cosmo.exe...
"%OBJCOPY%" -S -O binary cosmo.exe.dbg cosmo.exe
if errorlevel 1 goto error

REM 显示 EXE 大小
echo EXE size:
dir cosmo.exe | findstr "cosmo.exe"

echo Build complete.
exit /b 0

:error
echo Build failed with error %errorlevel%
exit /b 1 