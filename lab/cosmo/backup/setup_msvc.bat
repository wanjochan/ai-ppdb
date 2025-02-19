@echo off
set "VSINSTALLDIR=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
if not exist "%VSINSTALLDIR%" (
    set "VSINSTALLDIR=C:\Program Files\Microsoft Visual Studio\2022\BuildTools"
)

if not exist "%VSINSTALLDIR%" (
    echo Visual Studio Build Tools not found!
    echo Please install from: https://visualstudio.microsoft.com/zh-hans/visual-cpp-build-tools/
    exit /b 1
)

call "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars64.bat"
echo Environment setup complete!

REM 测试编译器是否可用
where cl
if %errorlevel% equ 0 (
    echo CL.exe found and ready to use!
) else (
    echo CL.exe not found in PATH!
) 