@echo off
setlocal enabledelayedexpansion

REM 检查参数
if "%1"=="" (
    echo Usage: build_test_case.bat ^<test_file^>
    echo Example: build_test_case.bat test/white/infra/test_sync
    exit /b 1
)

REM 设置环境变量
set TEST_FILE=%1
set BUILD_DIR=build\test
set TEST_NAME=%~n1

REM 创建构建目录
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

REM 编译测试用例
echo Building test case: %TEST_NAME%
clang -o %BUILD_DIR%\%TEST_NAME%.exe ^
    %TEST_FILE%.c ^
    src/test/test_framework.c ^
    -I include ^
    -I src ^
    -D_DEBUG ^
    -g

if %errorlevel% neq 0 (
    echo Build failed
    exit /b 1
)

echo Build successful: %BUILD_DIR%\%TEST_NAME%.exe 