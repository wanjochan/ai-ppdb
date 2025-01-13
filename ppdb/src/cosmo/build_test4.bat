@echo off
setlocal EnableDelayedExpansion

del /q *.exe

REM 加载环境变量和通用函数
call "..\..\..\ppdb\scripts\build_env.bat"
if errorlevel 1 exit /b 1

REM 验证 objcopy 是否可用
if not exist "%OBJCOPY%" (
    echo Error: OBJCOPY not found at %OBJCOPY%
    exit /b 1
)

REM 设置编译器和选项
set CFLAGS=%CFLAGS% -fPIC
set LDFLAGS_DL=-nostdlib -nostartfiles -shared -Wl,-T,dll.lds -Wl,--version-script=exports.txt -Wl,--no-undefined -Wl,-Bsymbolic

REM 编译动态库
echo Building test4.dll...
"%GCC%" %CFLAGS% -c test4.c -o test4.o
"%GCC%" %LDFLAGS_DL% test4.o -o test4.dll.dbg
"%OBJCOPY%" -S -O binary test4.dll.dbg test4.dll

REM 编译测试程序
echo Building test4_main...
"%GCC%" %CFLAGS% -c test4_main.c -o test4_main.o
"%GCC%" %LDFLAGS% test4_main.o %LIBS% -o test4_main.exe.dbg

REM 使用 objcopy 处理可执行文件
echo Creating binary test4_main...
echo Using OBJCOPY: %OBJCOPY%
"%OBJCOPY%" -S -O binary test4_main.exe.dbg test4_main.exe
if errorlevel 1 (
    echo Error: objcopy failed
    exit /b 1
)

echo Build complete.
endlocal 