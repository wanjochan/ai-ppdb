@echo off
setlocal enabledelayedexpansion

:: Change to script directory
cd %~dp0\..

set COSMO=cosmopolitan
set CROSS9=cross9\bin
set GCC=%CROSS9%\x86_64-pc-linux-gnu-gcc.exe
set AR=%CROSS9%\x86_64-pc-linux-gnu-ar.exe

:: Compilation options
set COMMON_FLAGS=-Wall -Wextra -fno-pie -fno-stack-protector -fno-omit-frame-pointer
set DEBUG_FLAGS=-g -O0 -DDEBUG
set RELEASE_FLAGS=-O2 -DNDEBUG
set CFLAGS=%COMMON_FLAGS% %DEBUG_FLAGS% -Iinclude -I%COSMO% -Isrc

:: Linker options
set LDFLAGS=-static -nostdlib -Wl,-T,%COSMO%\ape.lds -Wl,--gc-sections -fuse-ld=bfd
set LIBS=%COSMO%\crt.o %COSMO%\ape.o %COSMO%\cosmopolitan.a

:: Check toolchain
if not exist "%GCC%" (
    echo Error: Cross9 GCC not found at: %GCC%
    exit /b 1
)

:: Create build directory
if not exist build (
    echo Creating build directory...
    mkdir build
)

:: Create object directory
if not exist build\obj (
    echo Creating object directory...
    mkdir build\obj
)

cd build\obj

:: Compile common modules
echo Compiling common modules...
for %%f in (..\..\src\common\*.c) do (
    echo   Compiling %%f...
    "%GCC%" %CFLAGS% -c "%%f"
    if errorlevel 1 (
        echo Error compiling %%f
        cd ..\..\
        exit /b 1
    )
)

:: Compile KVStore modules
echo Compiling KVStore modules...
for %%f in (..\..\src\kvstore\*.c) do (
    echo   Compiling %%f...
    "%GCC%" %CFLAGS% -c "%%f"
    if errorlevel 1 (
        echo Error compiling %%f
        cd ..\..\
        exit /b 1
    )
)

:: Compile main program
echo Compiling main program...
"%GCC%" %CFLAGS% -c ..\..\src\main.c
if errorlevel 1 (
    echo Error compiling main.c
    cd ..\..\
    exit /b 1
)

:: Create static library
echo Creating static library...
"%AR%" rcs ..\libppdb.a *.o

:: Link executable
echo Linking executable...
"%GCC%" %LDFLAGS% *.o %LIBS% -o ..\ppdb.exe

:: Add APE self-modify support
echo Adding APE self-modify support...
copy /b ..\ppdb.exe + %COSMO%\ape-copy-self.o ..\ppdb.com
if errorlevel 1 (
    echo Error adding APE support
    cd ..\..\
    exit /b 1
)

:: Clean up intermediate files
echo Cleaning up...
cd ..
rmdir /s /q obj

echo Build completed successfully!
echo Binary: ppdb.com

cd ..
