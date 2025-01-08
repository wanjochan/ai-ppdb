@echo off
setlocal EnableDelayedExpansion

rem Get build mode from parameter
set "BUILD_MODE=%1"
if "%BUILD_MODE%"=="" set "BUILD_MODE=release"

rem Load environment variables and common functions
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

rem Create build directory if it doesn't exist
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem Build AST
echo Building AST...
"%GCC%" %CFLAGS% "%SRC_DIR%\ast.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\ast.exe.dbg"
if errorlevel 1 exit /b 1

"%OBJCOPY%" -S -O binary "%BUILD_DIR%\ast.exe.dbg" "%BUILD_DIR%\ast.exe"
if errorlevel 1 exit /b 1

rem Run tests if not explicitly disabled
if not "%2"=="norun" (
    echo.
    echo Testing number...
    echo Input: 42
    "%BUILD_DIR%\ast.exe" "42"

    echo.
    echo Testing add...
    echo Input: +(1,2)
    "%BUILD_DIR%\ast.exe" "+(1,2)"

    echo.
    echo Testing if...
    echo Input: if(1,42,0)
    "%BUILD_DIR%\ast.exe" "if(1,42,0)"

    echo.
    echo Testing local...
    echo Input: local(x,42)
    "%BUILD_DIR%\ast.exe" "local(x,42)"

    echo.
    echo Testing simple lambda...
    echo Input: lambda(x,x)
    "%BUILD_DIR%\ast.exe" "lambda(x,x)"

    echo.
    echo Testing simple call...
    echo Input: local(id,lambda(x,x),id(42))
    "%BUILD_DIR%\ast.exe" "local(id,lambda(x,x),id(42))"

    echo.
    echo Testing fibonacci...
    echo Input: local(fib,lambda(n,if(+(n,-1),if(+(n,-2),+(fib(+(n,-1)),fib(+(n,-2))),1),1)),fib(5))
    "%BUILD_DIR%\ast.exe" "local(fib,lambda(n,if(+(n,-1),if(+(n,-2),+(fib(+(n,-1)),fib(+(n,-2))),1),1)),fib(5))"
)

exit /b 0 