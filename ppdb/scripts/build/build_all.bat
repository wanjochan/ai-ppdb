@echo off
setlocal

echo Building PPDB...

REM 构建库
call "%~dp0\build_lib.bat"
if errorlevel 1 (
    echo Library build failed!
    exit /b 1
)

REM 构建测试
call "%~dp0\build_test.bat"
if errorlevel 1 (
    echo Test build failed!
    exit /b 1
)

echo All builds completed successfully!
exit /b 0
