@echo off
setlocal

set MODULE=%1
if "%MODULE%"=="" (
    echo Please specify a module to build
    exit /b 1
)

set BUILD_DIR=d:\dev\ai-ppdb\cosmo\build
set GCC=d:\dev\ai-ppdb\cross9\bin\x86_64-pc-linux-gnu-gcc.exe
set OBJCOPY=d:\dev\ai-ppdb\cross9\bin\x86_64-pc-linux-gnu-objcopy.exe

set CFLAGS=-Wall -Wextra -fno-pie -fno-stack-protector -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -fno-asynchronous-unwind-tables -g -O0 -DDEBUG -nostdinc -ffunction-sections

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo Copying Cosmopolitan files...
if not exist "%BUILD_DIR%\cosmopolitan.h" copy /y d:\dev\ai-ppdb\cosmopolitan\cosmopolitan.h "%BUILD_DIR%"
if not exist "%BUILD_DIR%\cosmopolitan.a" copy /y d:\dev\ai-ppdb\cosmopolitan\cosmopolitan.a "%BUILD_DIR%"
if not exist "%BUILD_DIR%\ape.o" copy /y d:\dev\ai-ppdb\cosmopolitan\ape.o "%BUILD_DIR%"
if not exist "%BUILD_DIR%\crt.o" copy /y d:\dev\ai-ppdb\cosmopolitan\crt.o "%BUILD_DIR%"
if not exist "%BUILD_DIR%\ape.lds" copy /y d:\dev\ai-ppdb\cosmopolitan\ape.lds "%BUILD_DIR%"

echo Building %MODULE%.exe...
if "%MODULE%"=="cosmo" (
    "%GCC%" %CFLAGS% -Id:\dev\ai-ppdb\cosmopolitan -include d:\dev\ai-ppdb\cosmopolitan\cosmopolitan.h -static -nostdlib -Wl,-T,d:\dev\ai-ppdb\cosmo\build\ape.lds -Wl,--gc-sections -fuse-ld=bfd -Wl,-z,max-page-size=0x1000 -no-pie -o "%BUILD_DIR%\%MODULE%.dbg" %MODULE%.c "%BUILD_DIR%\crt.o" "%BUILD_DIR%\ape.o" "%BUILD_DIR%\cosmopolitan.a"
    if errorlevel 1 goto :error

    echo Converting to APE format...
    "%OBJCOPY%" -O binary "%BUILD_DIR%\%MODULE%.dbg" %MODULE%.exe
    if errorlevel 1 goto :error

    echo Build %MODULE% completed successfully.
) else (
    echo "compile to %BUILD_DIR%\main.o"
    "%GCC%" %CFLAGS% -g -fvisibility=default -fPIC -Id:\dev\ai-ppdb\cosmopolitan -c main.c -o "%BUILD_DIR%\main.o"
    if errorlevel 1 goto :error

    echo "Creating exports.txt..."
    echo VERS_1.0 { > "%BUILD_DIR%\exports.txt"
    echo   global: module_main; >> "%BUILD_DIR%\exports.txt"
    echo   local: *; >> "%BUILD_DIR%\exports.txt"
    echo }; >> "%BUILD_DIR%\exports.txt"

    echo "Creating main.dat..."
    "%GCC%" -g -shared -nostartfiles -nostdlib -rdynamic -Wl,--export-dynamic -Wl,--emit-relocs -Wl,--entry=module_main -Wl,--gc-sections=off -Wl,-T,%BUILD_DIR%\ape.lds -o "%BUILD_DIR%\main.dbg" "%BUILD_DIR%\main.o" "%BUILD_DIR%\crt.o" "%BUILD_DIR%\ape.o" "%BUILD_DIR%\cosmopolitan.a" -Wl,--version-script="%BUILD_DIR%\exports.txt"
    if errorlevel 1 goto :error

    echo "Converting to APE format..."
    "%OBJCOPY%" -O binary "%BUILD_DIR%\main.dbg" main.dat
    if errorlevel 1 goto :error

    echo Build completed successfully.
)

goto :eof

:error
echo Build failed with error code %errorlevel%
exit /b %errorlevel%
