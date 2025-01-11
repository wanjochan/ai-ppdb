@echo off
setlocal enabledelayedexpansion

REM 设置环境变量
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

REM 确保 infra 库已经构建
if not exist "%BUILD_DIR%\infra\libinfra.a" (
    echo Building infra library first...
    call "%~dp0\build_infra.bat"
    if errorlevel 1 exit /b 1
)

REM 创建构建目录
if not exist "%BUILD_DIR%\test\white\framework\mock_framework" mkdir "%BUILD_DIR%\test\white\framework\mock_framework"
if not exist "%BUILD_DIR%\test\white\infra\mock\memory" mkdir "%BUILD_DIR%\test\white\infra\mock\memory"

REM 编译 mock 框架
echo Building mock framework...
"%GCC%" %CFLAGS% -I"%SRC_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" ^
    "%PPDB_DIR%\test\white\framework\mock_framework\mock_framework.c" ^
    -c -o "%BUILD_DIR%\test\white\framework\mock_framework\mock_framework.o"
if errorlevel 1 exit /b 1

REM 编译 memory mock
echo Building memory mock...
"%GCC%" %CFLAGS% -I"%SRC_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" ^
    "%PPDB_DIR%\test\white\infra\mock\memory\mock_memory.c" ^
    -c -o "%BUILD_DIR%\test\white\infra\mock\memory\mock_memory.o"
if errorlevel 1 exit /b 1

REM 编译 memory mock 测试
echo Building memory mock tests...
"%GCC%" %CFLAGS% -I"%SRC_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" ^
    "%PPDB_DIR%\test\white\infra\mock\memory\test_memory_mock.c" ^
    -c -o "%BUILD_DIR%\test\white\infra\mock\memory\test_memory_mock.o"
if errorlevel 1 exit /b 1

REM 链接测试可执行文件
echo Linking memory mock tests...
"%GCC%" %LDFLAGS% ^
    "%BUILD_DIR%\test\white\framework\mock_framework\mock_framework.o" ^
    "%BUILD_DIR%\test\white\infra\mock\memory\mock_memory.o" ^
    "%BUILD_DIR%\test\white\infra\mock\memory\test_memory_mock.o" ^
    "%BUILD_DIR%\infra\libinfra.a" ^
    %LIBS% ^
    -o "%BUILD_DIR%\test\white\infra\mock\memory\test_memory_mock.exe.dbg"
if errorlevel 1 exit /b 1

REM 创建最终可执行文件
"%OBJCOPY%" -S -O binary "%BUILD_DIR%\test\white\infra\mock\memory\test_memory_mock.exe.dbg" ^
    "%BUILD_DIR%\test\white\infra\mock\memory\test_memory_mock.exe"
if errorlevel 1 exit /b 1

REM 运行测试
echo Running memory mock tests...
"%BUILD_DIR%\test\white\infra\mock\memory\test_memory_mock.exe"
if errorlevel 1 (
    echo Memory mock tests FAILED
    exit /b 1
) else (
    echo Memory mock tests PASSED
)

exit /b 0 