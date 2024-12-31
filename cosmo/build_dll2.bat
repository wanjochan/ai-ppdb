@echo off
setlocal

set BUILD_DIR=d:\dev\ai-ppdb\cosmo\build
set MINGW_DIR=d:\dev\ai-ppdb\mingw64\bin
set GCC=%MINGW_DIR%\gcc.exe

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo Building runtime...
"%GCC%" -c -o "%BUILD_DIR%\runtime.o" runtime.c

echo Building main2.dll...
"%GCC%" -shared -o "%BUILD_DIR%\main2.dll" main2.c "%BUILD_DIR%\runtime.o" -lkernel32

echo Copying DLL...
copy /y "%BUILD_DIR%\main2.dll" main2.dll

echo Build completed successfully.
goto :eof

:error
echo Build failed with error code %errorlevel%
exit /b %errorlevel% 