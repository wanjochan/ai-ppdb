@echo off
setlocal

rem Set environment
call build_env.bat
if errorlevel 1 exit /b 1

rem Create build directories
if not exist ..\build mkdir ..\build
if not exist ..\build\infra mkdir ..\build\infra
if not exist ..\build\test mkdir ..\build\test
if not exist ..\build\test\white mkdir ..\build\test\white
if not exist ..\build\test\white\infra mkdir ..\build\test\white\infra

rem Build infra layer
echo Building infra layer...
%GCC% %CFLAGS% -c ..\src\infra\infra_core.c -o ..\build\infra\infra_core.o
%GCC% %CFLAGS% -c ..\src\infra\infra_struct.c -o ..\build\infra\infra_struct.o
%GCC% %CFLAGS% -c ..\src\infra\infra_sync.c -o ..\build\infra\infra_sync.o
%GCC% %CFLAGS% -c ..\src\infra\infra_async.c -o ..\build\infra\infra_async.o
%GCC% %CFLAGS% -c ..\src\infra\infra_event.c -o ..\build\infra\infra_event.o
%GCC% %CFLAGS% -c ..\src\infra\infra_io.c -o ..\build\infra\infra_io.o
%GCC% %CFLAGS% -c ..\src\infra\infra_buffer.c -o ..\build\infra\infra_buffer.o
%GCC% %CFLAGS% -c ..\src\infra\infra_peer.c -o ..\build\infra\infra_peer.o

rem Create static library
%AR% rcs ..\build\infra\libinfra.a ^
    ..\build\infra\infra_core.o ^
    ..\build\infra\infra_struct.o ^
    ..\build\infra\infra_sync.o ^
    ..\build\infra\infra_async.o ^
    ..\build\infra\infra_event.o ^
    ..\build\infra\infra_io.o ^
    ..\build\infra\infra_buffer.o ^
    ..\build\infra\infra_peer.o

rem Build tests
echo Building tests...
%GCC% %CFLAGS% -c ..\test\white\infra\test_infra.c -o ..\build\test\white\infra\test_infra.o

rem Link test executable
%GCC% %LDFLAGS% -o ..\build\test\white\infra\test_infra.com.dbg ^
    ..\build\test\white\infra\test_infra.o ^
    ..\build\infra\libinfra.a ^
    %LIBS%

rem Create COM file
%OBJCOPY% -S -O binary ..\build\test\white\infra\test_infra.com.dbg ..\build\test\white\infra\test_infra.com

echo Build complete!

rem Run tests if requested
if "%1"=="test" (
    echo Running tests...
    ..\build\test\white\infra\test_infra.com
) 