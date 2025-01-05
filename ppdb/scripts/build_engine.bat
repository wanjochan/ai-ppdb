@echo off
setlocal

set CC=gcc
set CFLAGS=-Wall -Wextra -I../../include -g
set LDFLAGS=-L../../lib

cd ..\test\white\engine

echo Building engine tests...

echo ===== Running Engine Layer Tests =====

REM Build and run each test
for %%f in (test_*.c) do (
    echo Building %%f...
    %CC% %CFLAGS% %%f -o %%~nf.exe %LDFLAGS%
    if errorlevel 1 (
        echo Failed to build %%f
        exit /b 1
    )
    
    echo Running %%f...
    %%~nf.exe
    if errorlevel 1 (
        echo Test %%f failed
        exit /b 1
    )
    del %%~nf.exe
)

cd ..\..\..

echo ===== All Engine Layer Tests Passed =====
exit /b 0
