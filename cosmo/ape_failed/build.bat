@echo off
setlocal

set MINGW_DIR=d:\dev\ai-ppdb\mingw64\bin
set GCC=%MINGW_DIR%\gcc.exe

echo Building APE generator...
"%GCC%" -c -o ape.o ape.c
if errorlevel 1 goto error

echo Building test program...
"%GCC%" -o test_ape.exe test_ape.c ape.o
if errorlevel 1 goto error

echo Build completed successfully.
goto :eof

:error
echo Build failed with error code %errorlevel%
exit /b %errorlevel% 