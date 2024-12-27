@echo off
setlocal enabledelayedexpansion

echo PPDB Test Build v1.0.0 (Debug)
echo Build time: %date% %time%
echo.

echo Checking environment...
if not exist "%~dp0..\src" (
    echo Error: Source directory not found
    exit /b 1
)

echo Setting up build flags...
set CFLAGS=-g -Wall -Wextra -I%~dp0..\include -DPPDB_TEST_MODE
set LDFLAGS=-lpthread

echo Preparing build directory...
if not exist "%~dp0..\build\test" mkdir "%~dp0..\build\test"
cd "%~dp0..\build\test"

echo Building source files...
echo Compiling common files...
if not exist "fs.o" (
    gcc %CFLAGS% -c "%~dp0..\src\common\fs.c"
    if !errorlevel! neq 0 exit /b !errorlevel!
) else (
    echo   fs.c [Skipped - up to date]
)

if not exist "logger.o" (
    gcc %CFLAGS% -c "%~dp0..\src\common\logger.c"
    if !errorlevel! neq 0 exit /b !errorlevel!
) else (
    echo   logger.c [Skipped - up to date]
)

echo Compiling kvstore files...
gcc %CFLAGS% -c "%~dp0..\src\kvstore\kvstore.c"
if !errorlevel! neq 0 exit /b !errorlevel!

gcc %CFLAGS% -c "%~dp0..\src\kvstore\memtable.c"
if !errorlevel! neq 0 exit /b !errorlevel!

if not exist "skiplist.o" (
    gcc %CFLAGS% -c "%~dp0..\src\kvstore\skiplist.c"
    if !errorlevel! neq 0 exit /b !errorlevel!
) else (
    echo   skiplist.c [Skipped - up to date]
)

gcc %CFLAGS% -c "%~dp0..\src\kvstore\wal.c"
if !errorlevel! neq 0 exit /b !errorlevel!

echo Building test files...
echo Compiling test framework...
if not exist "test_framework.o" (
    gcc %CFLAGS% -c "%~dp0..\test_white\test_framework.c"
    if !errorlevel! neq 0 exit /b !errorlevel!
) else (
    echo   test_framework.c [Skipped - up to date]
)

echo Building WAL test...
gcc %CFLAGS% -c "%~dp0..\test_white\test_wal.c"
if !errorlevel! neq 0 exit /b !errorlevel!
echo Creating WAL test binary...
gcc -o test_wal.exe test_wal.o wal.o fs.o logger.o test_framework.o %LDFLAGS%
if !errorlevel! neq 0 exit /b !errorlevel!

echo Building MemTable test...
gcc %CFLAGS% -c "%~dp0..\test_white\test_memtable.c"
if !errorlevel! neq 0 exit /b !errorlevel!
echo Creating MemTable test binary...
gcc -o test_memtable.exe test_memtable.o memtable.o skiplist.o logger.o test_framework.o %LDFLAGS%
if !errorlevel! neq 0 exit /b !errorlevel!

echo Building KVStore test...
gcc %CFLAGS% -c "%~dp0..\test_white\test_kvstore.c"
if !errorlevel! neq 0 exit /b !errorlevel!
echo Creating KVStore test binary...
gcc -o test_kvstore.exe test_kvstore.o kvstore.o memtable.o skiplist.o wal.o fs.o logger.o test_framework.o %LDFLAGS%
if !errorlevel! neq 0 exit /b !errorlevel!

echo Running tests...
echo Running WAL test...
.\test_wal.exe
if !errorlevel! neq 0 (
    echo WAL test failed with error code !errorlevel!
    echo Current directory: %CD%
    echo Build directory: %~dp0..\build\test
    exit /b !errorlevel!
)

echo Running MemTable test...
.\test_memtable.exe
if !errorlevel! neq 0 (
    echo MemTable test failed with error code !errorlevel!
    echo Current directory: %CD%
    echo Build directory: %~dp0..\build\test
    exit /b !errorlevel!
)

echo Running KVStore test...
.\test_kvstore.exe
if !errorlevel! neq 0 (
    echo KVStore test failed with error code !errorlevel!
    echo Current directory: %CD%
    echo Build directory: %~dp0..\build\test
    exit /b !errorlevel!
)

echo All tests passed successfully
exit /b 0 