@echo off
setlocal enabledelayedexpansion

set COSMO=..\cosmopolitan
set CROSS9=..\cross9\bin
set GCC=%CROSS9%\x86_64-pc-linux-gnu-gcc.exe
set AR=%CROSS9%\x86_64-pc-linux-gnu-ar.exe

:: Compilation options
set COMMON_FLAGS=-Wall -Wextra -fno-pie -fno-stack-protector -fno-omit-frame-pointer
set DEBUG_FLAGS=-g -O0 -DDEBUG
set RELEASE_FLAGS=-O2 -DNDEBUG
set CFLAGS=%COMMON_FLAGS% %DEBUG_FLAGS% -I ..\include -I %COSMO%

:: Linker options
set LDFLAGS=-static -nostdlib -Wl,-T,%COSMO%\ape.lds -Wl,--gc-sections -fuse-ld=bfd
set LIBS=%COSMO%\crt.o %COSMO%\ape.o %COSMO%\cosmopolitan.a

:: Check toolchain
if not exist "%GCC%" (
    echo Error: Cross9 GCC not found at: %GCC%
    echo Please check your toolchain installation
    exit /b 1
)

:: Create build directory
if not exist ..\build (
    echo Creating build directory...
    mkdir ..\build
)

:: Compile common modules
echo Compiling common modules...
for %%f in (..\src\common\*.c) do (
    echo   Compiling %%f...
    "%GCC%" %CFLAGS% -c "%%f"
    if errorlevel 1 (
        echo Error compiling %%f
        exit /b 1
    )
)

:: Compile KVStore modules
echo Compiling KVStore modules...
for %%f in (..\src\kvstore\*.c) do (
    echo   Compiling %%f...
    "%GCC%" %CFLAGS% -c "%%f"
    if errorlevel 1 (
        echo Error compiling %%f
        exit /b 1
    )
)

:: Compile main program
echo Compiling main program...
"%GCC%" %CFLAGS% -c ..\src\main.c
if errorlevel 1 (
    echo Error compiling main.c
    exit /b 1
)

:: Create static library
echo Creating static library...
"%AR%" rcs ..\build\libppdb.a *.o

:: Link executable
echo Linking executable...
"%GCC%" %LDFLAGS% *.o %LIBS% -o ..\build\ppdb.exe

:: Add APE self-modify support
echo Adding APE self-modify support...
copy /b ..\build\ppdb.exe + %COSMO%\ape-copy-self.o ..\build\ppdb.com
if errorlevel 1 (
    echo Error adding APE support
    exit /b 1
)

:: Clean up intermediate files
echo Cleaning up...
del *.o

echo Build completed successfully!
echo Binary: ..\build\ppdb.com
