@echo off
setlocal EnableDelayedExpansion

del /q *.exe *.dll *.bin *.o *.dbg *.map

REM 加载环境变量和通用函数
call "..\..\..\ppdb\scripts\build_env.bat"
if errorlevel 1 exit /b 1

REM 验证 objcopy 是否可用
if not exist "%OBJCOPY%" (
    echo Error: OBJCOPY not found at %OBJCOPY%
    exit /b 1
)

REM ========== DLL 编译部分 ==========
echo Building DLL...

REM DLL专用编译选项
set CFLAGS_DLL=%CFLAGS% -fPIC -fno-common -fno-plt -fno-semantic-interposition -D__COSMOPOLITAN__
set LDFLAGS_DLL=-nostdlib -nostartfiles -shared -Wl,-T,dll.lds -Wl,--version-script=exports.txt -Wl,--no-undefined -Wl,-Bsymbolic -fuse-ld=bfd -Wl,-z,notext -Wl,--build-id=none

REM 编译DLL
"%GCC%" %CFLAGS_DLL% -c test4.c -o test4.o
"%GCC%" %LDFLAGS_DLL% test4.o -o test4.dll.dbg
"%OBJCOPY%" -S -O binary test4.dll.dbg test4.dll

REM ========== EXE 编译部分 ==========
echo Building test program...

REM EXE专用编译选项
set CFLAGS_EXE=%CFLAGS% -D__COSMOPOLITAN__

REM 编译APE加载器
"%GCC%" %CFLAGS_EXE% -c ape_loader.c -o ape_loader.o

REM 编译测试程序
"%GCC%" %CFLAGS_EXE% -c test4_main.c -o test4_main.o
"%GCC%" %LDFLAGS% test4_main.o ape_loader.o %LIBS% -o test4_main.exe.dbg

REM 生成最终可执行文件
echo Creating binary test4_main...
echo Using OBJCOPY: %OBJCOPY%
"%OBJCOPY%" -S -O binary test4_main.exe.dbg test4_main.exe
if errorlevel 1 (
    echo Error: objcopy failed
    exit /b 1
)

echo Build complete.
endlocal 