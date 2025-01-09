@echo off
chcp 65001 > nul
setlocal EnableDelayedExpansion

:: Set proxy if provided
set "PROXY="
if not "%HTTP_PROXY%"=="" (
    set "PROXY=%HTTP_PROXY%"
) else if not "%HTTPS_PROXY%"=="" (
    set "PROXY=%HTTPS_PROXY%"
) else if not "%~1"=="" (
    set "PROXY=%~1"
)

if not "%PROXY%"=="" (
    echo Using proxy: %PROXY%
)

:: Set paths
set "SCRIPT_DIR=%~dp0"
echo Initial dir: %CD%
echo Script dir: %SCRIPT_DIR%
cd /d "%SCRIPT_DIR%\..\..\"
set "ROOT_DIR=%CD%"
echo Root dir: %ROOT_DIR%

echo === PPDB 环境初始化脚本 ===
echo.

rem 创建必要的目录
if not exist "repos" mkdir repos
if not exist "ppdb\tools" mkdir ppdb\tools
if not exist "ppdb\build" mkdir ppdb\build

rem 下载并安装工具链和运行时
echo.
echo 下载工具链和运行时...

rem 检查 cosmopolitan 是否完整
set "COSMO_COMPLETE=1"
if not exist "repos\cosmopolitan\cosmopolitan.h" set "COSMO_COMPLETE=0"
if not exist "repos\cosmopolitan\ape.lds" set "COSMO_COMPLETE=0"
if not exist "repos\cosmopolitan\crt.o" set "COSMO_COMPLETE=0"
if not exist "repos\cosmopolitan\ape.o" set "COSMO_COMPLETE=0"
if not exist "repos\cosmopolitan\cosmopolitan.a" set "COSMO_COMPLETE=0"

rem 下载并安装 cosmopolitan
if "%COSMO_COMPLETE%"=="0" (
    echo cosmopolitan 不存在或不完整，开始下载...
    if exist "repos\cosmopolitan" rd /s /q "repos\cosmopolitan"
    mkdir "repos\cosmopolitan"
    if exist "repos\cosmopolitan.zip" del /f /q "repos\cosmopolitan.zip"
    echo 下载 cosmopolitan...
    if not "%PROXY%"=="" (
        curl --retry 10 --retry-delay 5 --retry-max-time 0 --retry-all-errors --connect-timeout 30 --max-time 600 -C - -x "%PROXY%" -L "https://justine.lol/cosmopolitan/cosmopolitan.zip" -o "repos\cosmopolitan.zip"
    ) else (
        curl --retry 10 --retry-delay 5 --retry-max-time 0 --retry-all-errors --connect-timeout 30 --max-time 600 -C - -L "https://justine.lol/cosmopolitan/cosmopolitan.zip" -o "repos\cosmopolitan.zip"
    )
    echo 解压 cosmopolitan...
    cd "repos"
    powershell -Command "Expand-Archive -Path 'cosmopolitan.zip' -DestinationPath 'cosmopolitan' -Force"
    cd /d "%ROOT_DIR%"
) else (
    echo cosmopolitan 已存在且完整，跳过
)

rem 下载并安装 cross9
if not exist "repos\cross9\bin\x86_64-pc-linux-gnu-gcc.exe" (
    echo cross9 不存在或不完整，开始下载...
    if exist "repos\cross9" rd /s /q "repos\cross9"
    if not exist "repos\cross9.zip" (
        echo 下载 cross9...
        if not "%PROXY%"=="" (
            curl --retry 10 --retry-delay 5 --retry-max-time 0 --retry-all-errors --connect-timeout 30 --max-time 600 -C - -x "%PROXY%" -L "https://justine.lol/linux-compiler-on-windows/cross9.zip" -o "repos\cross9.zip"
        ) else (
            curl --retry 10 --retry-delay 5 --retry-max-time 0 --retry-all-errors --connect-timeout 30 --max-time 600 -C - -L "https://justine.lol/linux-compiler-on-windows/cross9.zip" -o "repos\cross9.zip"
        )
    ) else (
        echo 使用已下载的 cross9.zip
    )
    echo 解压 cross9...
    cd repos
    powershell -Command "Expand-Archive -Path 'cross9.zip' -DestinationPath '.' -Force"
    cd /d "%ROOT_DIR%"
) else (
    echo cross9 已存在且完整，跳过
)

rem 检查 cosmopolitan 目录内容
echo.
echo 检查 cosmopolitan 目录内容：
dir "repos\cosmopolitan"

rem 检查必要文件是否存在（核心文件必须检查）
if not exist "repos\cosmopolitan\cosmopolitan.h" (
    echo 错误：cosmopolitan.h 未找到
    exit /b 1
)
if not exist "repos\cosmopolitan\ape.lds" (
    echo 错误：ape.lds 未找到
    exit /b 1
)
if not exist "repos\cosmopolitan\crt.o" (
    echo 错误：crt.o 未找到
    exit /b 1
)
if not exist "repos\cosmopolitan\ape.o" (
    echo 错误：ape.o 未找到
    exit /b 1
)
if not exist "repos\cosmopolitan\cosmopolitan.a" (
    echo 错误：cosmopolitan.a 未找到
    exit /b 1
)

rem 复制运行时文件到构建目录
echo.
echo 准备构建目录...
echo 复制运行时文件到构建目录...
copy /Y "repos\cosmopolitan\ape.lds" "ppdb\build\"
copy /Y "repos\cosmopolitan\crt.o" "ppdb\build\"
copy /Y "repos\cosmopolitan\ape.o" "ppdb\build\"
copy /Y "repos\cosmopolitan\cosmopolitan.a" "ppdb\build\"

echo 检查构建目录内容：
dir "ppdb\build"

rem 验证环境
echo.
echo 验证环境...

rem 检查工具链
if not exist "repos\cross9\bin\x86_64-pc-linux-gnu-gcc.exe" (
    echo 错误：cross9 未正确安装
    exit /b 1
)

rem 检查运行时文件
if not exist "ppdb\build\ape.lds" (
    echo 错误：运行时文件未正确安装
    exit /b 1
)

if not exist "repos\cosmopolitan\cosmopolitan.h" (
    echo 错误：cosmopolitan.h 未找到
    exit /b 1
)

rem 运行测试
echo.
echo 运行测试...
echo Before pushd: %CD%
pushd ppdb
echo After pushd: %CD%
call scripts\build.bat test42
popd
echo After popd: %CD%

if errorlevel 1 (
    echo 错误：测试失败
    exit /b 1
)

echo.
echo ===================================
echo     all set
echo ===================================

cd /d "%SCRIPT_DIR%"
echo Final dir: %CD%

exit /b 0 
