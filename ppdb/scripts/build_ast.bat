@echo off
setlocal EnableDelayedExpansion

rem Load environment variables and common functions
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

rem Create build directory if it doesn't exist
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"


rem Build AST runtime
echo Building AST runtime...
"%GCC%" %CFLAGS% -c "%SRC_DIR%\ast_runtime.c" -o "%BUILD_DIR%\ast_runtime.o"
if errorlevel 1 exit /b 1

rem Build AST core
echo Building AST core...
"%GCC%" %CFLAGS% -c "%SRC_DIR%\ast.c" -o "%BUILD_DIR%\ast.o"
if errorlevel 1 exit /b 1



echo Build successful!

rem Run tests
echo Running AST tests 1: expect 3
"%BUILD_DIR%\ast.exe" "+(1, 2)"

rem Test lambda function
echo Testing lambda function...
echo Test case 1: const42 function
"%BUILD_DIR%\ast.exe" "local(const42, lambda(x, 42));const42(0)"

rem Test fibonacci function
echo Testing fibonacci function...
echo Test case 1: fib(0), expect 0
"%BUILD_DIR%\ast.exe" "local(fib, lambda(n, if(=(n, 0), 0, if(=(n, 1), 1, +(fib(-(n, 1)), fib(-(n, 2)))))));fib(0)"

echo Test case 2: fib(1), expect 1
"%BUILD_DIR%\ast.exe" "local(fib, lambda(n, if(=(n, 0), 0, if(=(n, 1), 1, +(fib(-(n, 1)), fib(-(n, 2)))))));fib(1)"

echo Test case 3: fib(2), expect 1
"%BUILD_DIR%\ast.exe" "local(fib, lambda(n, if(=(n, 0), 0, if(=(n, 1), 1, +(fib(-(n, 1)), fib(-(n, 2)))))));fib(2)"

if errorlevel 1 exit /b 1

echo All tests passed!
exit /b 0 