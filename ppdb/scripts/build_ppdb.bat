@echo off
setlocal enabledelayedexpansion

:: Set root paths
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..
set WORKSPACE_ROOT=%PROJECT_ROOT%\..
set BUILD_DIR=%PROJECT_ROOT%\build
set SRC_DIR=%PROJECT_ROOT%\src
set INCLUDE_DIR=%PROJECT_ROOT%\include

:: Set dependency paths (absolute paths)
set COSMO=%WORKSPACE_ROOT%\cosmopolitan
set CROSS9=%WORKSPACE_ROOT%\cross9\bin
set GCC=%CROSS9%\x86_64-pc-linux-gnu-gcc.exe
set AR=%CROSS9%\x86_64-pc-linux-gnu-ar.exe

:: Debug paths
echo Checking paths:
echo WORKSPACE_ROOT: %WORKSPACE_ROOT%
echo COSMO: %COSMO%
echo GCC: %GCC%

:: Check required directories and files
if not exist "%COSMO%" (
    echo Error: Cosmopolitan directory not found at: %COSMO%
    exit /b 1
)

if not exist "%CROSS9%" (
    echo Error: Cross9 directory not found at: %CROSS9%
    exit /b 1
)

if not exist "%GCC%" (
    echo Error: GCC not found at: %GCC%
    exit /b 1
)

if not exist "%AR%" (
    echo Error: AR not found at: %AR%
    exit /b 1
)

:: Compilation options
set COMMON_FLAGS=-Wall -Wextra -fno-pie -fno-stack-protector -fno-omit-frame-pointer
set DEBUG_FLAGS=-g -O0 -DDEBUG
set RELEASE_FLAGS=-O2 -DNDEBUG
set CFLAGS=%COMMON_FLAGS% %DEBUG_FLAGS% -I"%INCLUDE_DIR%" -I"%COSMO%" -I"%SRC_DIR%" -I"%PROJECT_ROOT%"

:: Linker options
set LDFLAGS=-static -nostdlib -Wl,-T,"%COSMO%\ape.lds" -Wl,--gc-sections -fuse-ld=bfd
set LIBS="%COSMO%\crt.o" "%COSMO%\ape.o" "%COSMO%\cosmopolitan.a"

:: Create build directory
if not exist "%BUILD_DIR%" (
    echo Creating build directory...
    mkdir "%BUILD_DIR%"
)

:: Create object directory
set OBJ_DIR=%BUILD_DIR%\obj
if not exist "%OBJ_DIR%" (
    echo Creating object directory...
    mkdir "%OBJ_DIR%"
)

:: Change to object directory for compilation
pushd "%OBJ_DIR%"

:: Compile common modules
echo Compiling common modules...
for %%f in ("%SRC_DIR%\common\*.c") do (
    echo   Compiling %%f...
    pushd "%SRC_DIR%"
    "%GCC%" %CFLAGS% -c "%%f" -o "%OBJ_DIR%\%%~nf.o"
    popd
    if errorlevel 1 (
        echo Error compiling %%f
        popd
        exit /b 1
    )
)

:: Compile KVStore modules
echo Compiling KVStore modules...
for %%f in ("%SRC_DIR%\kvstore\*.c") do (
    echo   Compiling %%f...
    pushd "%SRC_DIR%"
    "%GCC%" %CFLAGS% -c "%%f" -o "%OBJ_DIR%\%%~nf.o"
    popd
    if errorlevel 1 (
        echo Error compiling %%f
        popd
        exit /b 1
    )
)

:: Compile main program
echo Compiling main program...
pushd "%SRC_DIR%"
"%GCC%" %CFLAGS% -c "main.c" -o "%OBJ_DIR%\main.o"
popd
if errorlevel 1 (
    echo Error compiling main.c
    popd
    exit /b 1
)

:: Create static library
echo Creating static library...
"%AR%" rcs "%BUILD_DIR%\libppdb.a" *.o

:: Link executable
echo Linking executable...
"%GCC%" %LDFLAGS% *.o %LIBS% -o "%BUILD_DIR%\ppdb.exe"

:: Add APE self-modify support
echo Adding APE self-modify support...
copy /b "%BUILD_DIR%\ppdb.exe" + "%COSMO%\ape-copy-self.o" "%BUILD_DIR%\ppdb.com"
if errorlevel 1 (
    echo Error adding APE support
    popd
    exit /b 1
)

:: Clean up intermediate files
echo Cleaning up...
popd
rmdir /s /q "%OBJ_DIR%"

echo Build completed successfully!
echo Binary: %BUILD_DIR%\ppdb.com
