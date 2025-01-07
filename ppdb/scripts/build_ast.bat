@echo off
setlocal

rem 加载构建环境
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

rem 创建构建目录
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem 编译AST运行时
"%GCC%" %CFLAGS% -c -o "%BUILD_DIR%\ast_runtime.o" "%SRC_DIR%\ast_runtime.c"
if errorlevel 1 exit /b 1

rem 编译AST
"%GCC%" %CFLAGS% -c -o "%BUILD_DIR%\ast.o" "%SRC_DIR%\ast.c"
if errorlevel 1 exit /b 1

rem 编译主程序
"%GCC%" %CFLAGS% -c -o "%BUILD_DIR%\ast_main.o" "%SRC_DIR%\ast_main.c"
if errorlevel 1 exit /b 1

rem 链接
"%GCC%" -o "%BUILD_DIR%\ast.exe.dbg" ^
    "%BUILD_DIR%\ast_runtime.o" ^
    "%BUILD_DIR%\ast.o" ^
    "%BUILD_DIR%\ast_main.o" ^
    %LDFLAGS% %LIBS%
if errorlevel 1 exit /b 1

rem 生成最终可执行文件
"%OBJCOPY%" -S -O binary "%BUILD_DIR%\ast.exe.dbg" "%BUILD_DIR%\ast.exe"
if errorlevel 1 exit /b 1

echo Build successful! 