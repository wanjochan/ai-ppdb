@echo off
setlocal

set MODULE=%1
if not exist "%MODULE%" set MODULE=cosmo

set BUILD_DIR=d:\dev\ai-ppdb\cosmo\build
set GCC=d:\dev\ai-ppdb\cross9\bin\x86_64-pc-linux-gnu-gcc.exe
set OBJCOPY=d:\dev\ai-ppdb\cross9\bin\x86_64-pc-linux-gnu-objcopy.exe

rem Common flags for both exe and dll
set COMMON_FLAGS=-Wall -Wextra -fno-stack-protector -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -fno-asynchronous-unwind-tables -g -O0 -DDEBUG -nostdinc -ffunction-sections -mcmodel=large

rem Specific flags for exe
set EXE_FLAGS=%COMMON_FLAGS% -fno-pie
rem Specific flags for dll
set DLL_FLAGS=%COMMON_FLAGS% -fPIC

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo Copying Cosmopolitan files...
if not exist "%BUILD_DIR%\cosmopolitan.h" copy /y d:\dev\ai-ppdb\cosmopolitan\cosmopolitan.h "%BUILD_DIR%"
if not exist "%BUILD_DIR%\cosmopolitan.a" copy /y d:\dev\ai-ppdb\cosmopolitan\cosmopolitan.a "%BUILD_DIR%"
if not exist "%BUILD_DIR%\ape.o" copy /y d:\dev\ai-ppdb\cosmopolitan\ape.o "%BUILD_DIR%"
if not exist "%BUILD_DIR%\crt.o" copy /y d:\dev\ai-ppdb\cosmopolitan\crt.o "%BUILD_DIR%"
if not exist "%BUILD_DIR%\ape.lds" copy /y d:\dev\ai-ppdb\cosmopolitan\ape.lds "%BUILD_DIR%"
if not exist "%BUILD_DIR%\dll.lds" copy /y d:\dev\ai-ppdb\cosmo\dll.lds "%BUILD_DIR%"

echo Building %MODULE%

if "%MODULE%"=="cosmo" (
    @rem TARGET is %MODULE%.exe, the loader (with cosmopolitan ape)
    "%GCC%" %EXE_FLAGS% -Id:\dev\ai-ppdb\cosmopolitan -include d:\dev\ai-ppdb\cosmopolitan\cosmopolitan.h -static -nostdlib -Wl,-T,"%BUILD_DIR%\ape.lds" -Wl,--gc-sections -fuse-ld=bfd -Wl,-z,max-page-size=0x1000 -no-pie -o "%BUILD_DIR%\%MODULE%.dbg" %MODULE%.c "%BUILD_DIR%\crt.o" "%BUILD_DIR%\ape.o" "%BUILD_DIR%\cosmopolitan.a"
    if errorlevel 1 goto :error

    echo Converting to APE format...
    "%OBJCOPY%" -O binary "%BUILD_DIR%\%MODULE%.dbg" %MODULE%.exe
    if errorlevel 1 goto :error
    dir "%MODULE%.exe"
    echo Build %MODULE% completed successfully.
) else (
    @rem TARGET %MODULE%.dat, the dl (our ape format for .dll/.so/.dyld)
    echo TODO
)

goto :eof

:error
echo Build failed with error code %errorlevel%
exit /b %errorlevel%

