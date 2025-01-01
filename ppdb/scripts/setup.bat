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
cd /d "%SCRIPT_DIR%\..\..\"
set "ROOT_DIR=%CD%"

echo === PPDB 环境初始化脚本 ===
echo.

rem 创建必要的目录
if not exist "repos" mkdir repos
if not exist "repos\cosmopolitan" mkdir repos\cosmopolitan
if not exist "ppdb\tools" mkdir ppdb\tools
if not exist "ppdb\build" mkdir ppdb\build

rem 下载并安装工具链
echo.
echo 下载工具链...

rem 下载并安装 cross9
if not exist "repos\cross9\bin\x86_64-pc-linux-gnu-gcc.exe" (
    if not exist "cross9.zip" (
        echo 下载 cross9...
        if not "%PROXY%"=="" (
            curl --retry 10 --retry-delay 5 --retry-max-time 0 --retry-all-errors --connect-timeout 30 --max-time 600 -C - -x "%PROXY%" -L "https://justine.lol/linux-compiler-on-windows/cross9.zip" -o cross9.zip
        ) else (
            curl --retry 10 --retry-delay 5 --retry-max-time 0 --retry-all-errors --connect-timeout 30 --max-time 600 -C - -L "https://justine.lol/linux-compiler-on-windows/cross9.zip" -o cross9.zip
        )
    ) else (
        echo 使用已下载的 cross9.zip
    )
    echo 解压 cross9...
    powershell -Command "Expand-Archive -Path 'cross9.zip' -DestinationPath 'repos\cross9' -Force"
    del /F /Q cross9.zip
) else (
    echo cross9 已存在，跳过
)

rem 克隆参考代码
echo.
echo 克隆参考代码...
cd repos

if not exist "leveldb" (
    echo 克隆 leveldb...
    if not "%PROXY%"=="" (
        git -c http.proxy="%PROXY%" clone --depth 1 --single-branch --no-tags https://github.com/google/leveldb.git
    ) else (
        git clone --depth 1 --single-branch --no-tags https://github.com/google/leveldb.git
    )
) else (
    echo leveldb 已存在，跳过
)

cd ..

rem 复制运行时文件到构建目录
echo.
echo 准备构建目录...
copy /Y "repos\cross9\x86_64-pc-linux-gnu\lib\ape.lds" "ppdb\build\"
copy /Y "repos\cross9\x86_64-pc-linux-gnu\lib\crt.o" "ppdb\build\"
copy /Y "repos\cross9\x86_64-pc-linux-gnu\lib\ape.o" "ppdb\build\"
copy /Y "repos\cross9\x86_64-pc-linux-gnu\lib\cosmopolitan.a" "ppdb\build\"

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

rem 运行测试
echo.
echo 运行测试...
cd ppdb
call scripts\build.bat test42
if errorlevel 1 (
    echo 错误：测试失败
    exit /b 1
)

echo.
echo === 环境初始化完成 ===
echo 你现在可以开始构建 PPDB 了
echo 运行 'scripts\build.bat help' 查看构建选项

exit /b 0 
