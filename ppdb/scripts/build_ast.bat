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
"%GCC%" %CFLAGS% ^
    "%SRC_DIR%\ast.c" ^
    %LDFLAGS% %LIBS% -o "%BUILD_DIR%\ast.exe.dbg"
if errorlevel 1 exit /b 1

"%OBJCOPY%" -S -O binary "%BUILD_DIR%\ast.exe.dbg" "%BUILD_DIR%\ast.exe"
if errorlevel 1 exit /b 1

rem Run the test if not explicitly disabled
if not "%2"=="norun" (
    echo.
    echo Test 1: Basic arithmetic
    echo Expression: add(1, 2)
    echo Expected: 3
    "%BUILD_DIR%\ast.exe" "add(1, 2)"
    echo.

    echo Test 2: Variable definition
    echo Expression: local(x, 42)
    echo Expected: 42
    "%BUILD_DIR%\ast.exe" "local(x, 42)"
    echo.

    echo Test 3: Variable lookup
    echo Expression: seq(local(x, 42), x)
    echo Expected: 42
    "%BUILD_DIR%\ast.exe" "seq(local(x, 42), x)"
    echo.

    echo Test 4: Function definition
    echo Expression: local(f, lambda(x, 42))
    echo Expected: lambda(x, 42)
    "%BUILD_DIR%\ast.exe" "local(f, lambda(x, 42))"
    echo.

    echo Test 5: Function lookup
    echo Expression: seq(local(f, lambda(x, 42)), f)
    echo Expected: lambda(x, 42)
    "%BUILD_DIR%\ast.exe" "seq(local(f, lambda(x, 42)), f)"
    echo.

    echo Test 6: Function call
    echo Expression: seq(local(f, lambda(x, 42)), f(0))
    echo Expected: 42
    "%BUILD_DIR%\ast.exe" "seq(local(f, lambda(x, 42)), f(0))"
)

exit /b 0 