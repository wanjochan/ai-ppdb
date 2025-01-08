@echo off
setlocal

echo Testing fibonacci with different depths...

echo.
echo Testing fib(5)...
..\build\ast2.exe "local(fib,lambda(n,if(+(n,-1),if(+(n,-2),+(fib(+(n,-1)),fib(+(n,-2))),1),1)),fib(5))"

echo.
echo Testing fib(15)...
..\build\ast2.exe "local(fib,lambda(n,if(+(n,-1),if(+(n,-2),+(fib(+(n,-1)),fib(+(n,-2))),1),1)),fib(15))"

echo.
echo Testing fib(27)... (should work - within limit)
..\build\ast2.exe "local(fib,lambda(n,if(+(n,-1),if(+(n,-2),+(fib(+(n,-1)),fib(+(n,-2))),1),1)),fib(27))"

echo.
echo Testing fib(28)... (should fail - exceeds limit)
..\build\ast2.exe "local(fib,lambda(n,if(+(n,-1),if(+(n,-2),+(fib(+(n,-1)),fib(+(n,-2))),1),1)),fib(28))"

echo.
echo Testing fib(30)... (should fail - exceeds limit)
..\build\ast2.exe "local(fib,lambda(n,if(+(n,-1),if(+(n,-2),+(fib(+(n,-1)),fib(+(n,-2))),1),1)),fib(30))" 