@echo off
setlocal enabledelayedexpansion

set COSMO=..\cosmopolitan
set CROSS9=..\cross9\bin
set GCC=%CROSS9%\x86_64-pc-linux-gnu-gcc.exe
set AR=%CROSS9%\x86_64-pc-linux-gnu-ar.exe

:: Compilation options
set COMMON_FLAGS=-Wall -Wextra -fno-pie -fno-stack-protector -fno-omit-frame-pointer
set DEBUG_FLAGS=-g -O0 -DDEBUG
set CFLAGS=%COMMON_FLAGS% %DEBUG_FLAGS% -I ..\include -I %COSMO%

:: Linker options
set LDFLAGS=-static -nostdlib -Wl,-T,%COSMO%\ape.lds -Wl,--gc-sections -fuse-ld=bfd
set LIBS=%COSMO%\crt.o %COSMO%\ape.o %COSMO%\cosmopolitan.a

:: Ensure library is built
if not exist ..\build\libppdb.a (
    echo Building library first...
    call build_ppdb.bat
    if errorlevel 1 (
        echo Error building library
        exit /b 1
    )
)

:: Compile test framework
echo Compiling test framework...
"%GCC%" %CFLAGS% -c ..\test\white\test_framework.c
if errorlevel 1 (
    echo Error compiling test framework
    exit /b 1
)

:: Compile test files
echo Compiling test files...
for %%f in (..\test\white\test_*.c) do (
    :: Skip test_framework.c and test_*_main.c
    echo %%f | findstr /i /c:"test_framework.c" /c:"test_.*_main.c" > nul
    if errorlevel 1 (
        echo   Compiling %%f...
        "%GCC%" %CFLAGS% -c "%%f"
        if errorlevel 1 (
            echo Error compiling %%f
            exit /b 1
        )
    )
)

:: Compile test main programs
echo Compiling test main programs...
for %%f in (..\test\white\test_*_main.c) do (
    echo   Compiling %%f...
    "%GCC%" %CFLAGS% -c "%%f"
    if errorlevel 1 (
        echo Error compiling %%f
        exit /b 1
    )
)

:: Link test programs
echo Linking test programs...
for %%f in (test_*_main.o) do (
    set "test_name=%%~nf"
    set "test_name=!test_name:_main=!"
    echo   Linking !test_name!.exe...
    "%GCC%" %LDFLAGS% "%%f" !test_name!.o test_framework.o ..\build\libppdb.a %LIBS% -o ..\build\!test_name!.exe
    
    :: Add APE self-modify support
    echo   Adding APE self-modify support to !test_name!...
    copy /b ..\build\!test_name!.exe + %COSMO%\ape-copy-self.o ..\build\!test_name!.com
    if errorlevel 1 (
        echo Error adding APE support to !test_name!
        exit /b 1
    )
)

:: Clean up intermediate files
echo Cleaning up...
del *.o

echo Test build completed successfully!
echo Test binaries in: ..\build\
