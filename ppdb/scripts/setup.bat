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
cd /d "%SCRIPT_DIR%\.."
set "ROOT_DIR=%CD%"

echo === PPDB 环境初始化脚本 ===
echo.

rem 创建必要的目录
if not exist "repos" mkdir repos
if not exist "repos\cosmopolitan" mkdir repos\cosmopolitan
if not exist "tools" mkdir tools
if not exist "build" mkdir build

rem 下载并安装工具链
echo.
echo 下载工具链...

rem 下载并安装 cosmocc
if not exist "tools\cosmocc\bin\cosmocc" (
    if not exist "cosmocc.zip" (
        echo 下载 cosmocc...
        if not "%PROXY%"=="" (
            curl -x "%PROXY%" -L "https://cosmo.zip/pub/cosmocc/cosmocc.zip" -o cosmocc.zip
        ) else (
            curl -L "https://cosmo.zip/pub/cosmocc/cosmocc.zip" -o cosmocc.zip
        )
    ) else (
        echo 使用已下载的 cosmocc.zip
    )
    echo 解压 cosmocc...
    powershell -Command "Expand-Archive -Path 'cosmocc.zip' -DestinationPath 'tools\cosmocc' -Force"
    echo 复制运行时文件...
    copy /Y "tools\cosmocc\bin\ape-x86_64.elf" "repos\cosmopolitan\ape.elf"
    copy /Y "tools\cosmocc\bin\ape-x86_64.elf" "repos\cosmopolitan\crt.o"
    copy /Y "tools\cosmocc\bin\ape-x86_64.elf" "repos\cosmopolitan\ape.o"
    copy /Y "tools\cosmocc\bin\ape-x86_64.elf" "repos\cosmopolitan\cosmopolitan.a"
    del /F /Q cosmocc.zip
) else (
    echo cosmocc 已存在，跳过
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
copy /Y "repos\cosmopolitan\ape.elf" "build\ape.lds"
copy /Y "repos\cosmopolitan\crt.o" "build\"
copy /Y "repos\cosmopolitan\ape.o" "build\"
copy /Y "repos\cosmopolitan\cosmopolitan.a" "build\"

rem 验证环境
echo.
echo 验证环境...

rem 检查工具链
if not exist "tools\cosmocc\bin\cosmocc" (
    echo 错误：cosmocc 未正确安装
    exit /b 1
)

rem 检查运行时文件
if not exist "repos\cosmopolitan\ape.elf" (
    echo 错误：cosmopolitan 运行时文件未正确安装
    exit /b 1
)

rem 验证编译器
echo int main() { return 0; } > test.c
tools\cosmocc\bin\cosmocc test.c -o test.com
if errorlevel 1 (
    echo 错误：编译测试失败
    del /F /Q test.c
    exit /b 1
)
del /F /Q test.c test.com

echo.
echo === 环境初始化完成 ===
echo 你现在可以开始构建 PPDB 了
echo 运行 'scripts\build.bat help' 查看构建选项

exit /b 0 
