@echo off
setlocal

set BUILD_DIR=d:\dev\ai-ppdb\cosmo\build
set GCC=d:\dev\ai-ppdb\cross9\bin\x86_64-pc-linux-gnu-gcc.exe
set OBJCOPY=d:\dev\ai-ppdb\cross9\bin\x86_64-pc-linux-gnu-objcopy.exe

set CFLAGS=-Wall -Wextra -fPIC -shared -fno-stack-protector -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -fno-asynchronous-unwind-tables -g -O0 -DDEBUG -nostdinc -ffunction-sections

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo Copying Cosmopolitan files...
if not exist "%BUILD_DIR%\cosmopolitan.h" copy /y d:\dev\ai-ppdb\cosmopolitan\cosmopolitan.h "%BUILD_DIR%"
if not exist "%BUILD_DIR%\cosmopolitan.a" copy /y d:\dev\ai-ppdb\cosmopolitan\cosmopolitan.a "%BUILD_DIR%"
if not exist "%BUILD_DIR%\ape.o" copy /y d:\dev\ai-ppdb\cosmopolitan\ape.o "%BUILD_DIR%"
if not exist "%BUILD_DIR%\crt.o" copy /y d:\dev\ai-ppdb\cosmopolitan\crt.o "%BUILD_DIR%"
if not exist "%BUILD_DIR%\ape.lds" copy /y d:\dev\ai-ppdb\cosmopolitan\ape.lds "%BUILD_DIR%"

echo Building main2.dll...
"%GCC%" %CFLAGS% -Id:\dev\ai-ppdb\cosmopolitan -include d:\dev\ai-ppdb\cosmopolitan\cosmopolitan.h -static -nostdlib -Wl,-T,d:\dev\ai-ppdb\cosmo\build\ape.lds -Wl,--gc-sections -fuse-ld=bfd -Wl,-z,max-page-size=0x1000 -o "%BUILD_DIR%\main2.dbg" main2.c "%BUILD_DIR%\crt.o" "%BUILD_DIR%\ape.o" "%BUILD_DIR%\cosmopolitan.a"
if errorlevel 1 goto :error

echo Converting to DLL format...
"%OBJCOPY%" -O binary "%BUILD_DIR%\main2.dbg" main2.dll
if errorlevel 1 goto :error

echo Build completed successfully.
goto :eof

:error
echo Build failed with error code %errorlevel%
exit /b %errorlevel% 