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

rem Build AST2
echo Building AST2...
"%GCC%" %CFLAGS% "%SRC_DIR%\ast2.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\ast2.exe.dbg"
if errorlevel 1 exit /b 1

"%OBJCOPY%" -S -O binary "%BUILD_DIR%\ast2.exe.dbg" "%BUILD_DIR%\ast2.exe"
if errorlevel 1 exit /b 1

rem Run tests if not explicitly disabled
if not "%2"=="norun" (
    echo.
    echo === Basic Tests ===
    echo Testing number...
    echo Input: 42
    "%BUILD_DIR%\ast2.exe" "42"

    echo.
    echo === Arithmetic Tests ===
    echo Testing add...
    echo Input: +(1,2)
    "%BUILD_DIR%\ast2.exe" "+(1,2)"

    echo.
    echo Testing multiply...
    echo Input: *(6,7)
    "%BUILD_DIR%\ast2.exe" "*(6,7)"

    echo.
    echo Testing divide...
    echo Input: /(10,2)
    "%BUILD_DIR%\ast2.exe" "/(10,2)"

    echo.
    echo Testing modulo...
    echo Input: mod(7,3)
    "%BUILD_DIR%\ast2.exe" "mod(7,3)"

    echo.
    echo Testing divide by zero...
    echo Input: /(1,0)
    "%BUILD_DIR%\ast2.exe" "/(1,0)"

    echo.
    echo === Special Form Tests ===
    echo Testing if with true condition...
    echo Input: if(1,42,0)
    "%BUILD_DIR%\ast2.exe" "if(1,42,0)"

    echo.
    echo Testing if with false condition...
    echo Input: if(0,42,7)
    "%BUILD_DIR%\ast2.exe" "if(0,42,7)"

    echo.
    echo Testing if with invalid condition...
    echo Input: if(+(1,1),42,0)
    "%BUILD_DIR%\ast2.exe" "if(+(1,1),42,0)"

    echo.
    echo Testing if with wrong argument count...
    echo Input: if(1,2)
    "%BUILD_DIR%\ast2.exe" "if(1,2)"

    echo.
    echo === Variable Tests ===
    echo Testing local variable...
    echo Input: local(x,42)
    "%BUILD_DIR%\ast2.exe" "local(x,42)"

    echo.
    echo Testing local with invalid name...
    echo Input: local(42,42)
    "%BUILD_DIR%\ast2.exe" "local(42,42)"

    echo.
    echo Testing undefined variable...
    echo Input: y
    "%BUILD_DIR%\ast2.exe" "y"

    echo.
    echo === Function Tests ===
    echo Testing simple lambda...
    echo Input: lambda(x,x)
    "%BUILD_DIR%\ast2.exe" "lambda(x,x)"

    echo.
    echo Testing lambda with invalid parameter...
    echo Input: lambda(42,x)
    "%BUILD_DIR%\ast2.exe" "lambda(42,x)"

    echo.
    echo Testing simple function call...
    echo Input: local(id,lambda(x,x),id(42))
    "%BUILD_DIR%\ast2.exe" "local(id,lambda(x,x),id(42))"

    echo.
    echo Testing recursive function...
    echo Input: local(fib,lambda(n,if(+(n,-1),if(+(n,-2),+(fib(+(n,-1)),fib(+(n,-2))),1),1)),fib(5))
    "%BUILD_DIR%\ast2.exe" "local(fib,lambda(n,if(+(n,-1),if(+(n,-2),+(fib(+(n,-1)),fib(+(n,-2))),1),1)),fib(5))"

    echo.
    echo === Complex Tests ===
    echo Testing nested if...
    echo Input: if(1,if(1,42,0),0)
    "%BUILD_DIR%\ast2.exe" "if(1,if(1,42,0),0)"

    echo.
    echo Testing nested local...
    echo Input: local(x,1,local(y,2,+(x,y)))
    "%BUILD_DIR%\ast2.exe" "local(x,1,local(y,2,+(x,y)))"

    echo.
    echo Testing function with multiple calls...
    echo Input: local(f,lambda(x,*(x,2)),+(f(3),f(4)))
    "%BUILD_DIR%\ast2.exe" "local(f,lambda(x,*(x,2)),+(f(3),f(4)))"
)

exit /b 0 