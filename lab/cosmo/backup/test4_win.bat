@echo off
REM 编译测试程序
cl /nologo test4_win.c

REM 如果编译成功，运行测试
if %errorlevel% equ 0 (
    echo.
    echo Running Windows native test...
    echo.
    test4_win.exe
) else (
    echo Compilation failed!
)

REM 显示DLL信息
echo.
echo Checking DLL information...
echo.
dumpbin /headers test4.dll | findstr /i "image"
dumpbin /headers test4.dll | findstr /i "base"
echo.
echo Exports:
dumpbin /exports test4.dll
echo.
echo Sections:
dumpbin /sections test4.dll 