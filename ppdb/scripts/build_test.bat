@echo off
setlocal enabledelayedexpansion

set COSMO=..\cosmopolitan
set CROSS9=..\cross9\bin
set GCC=%CROSS9%\x86_64-pc-linux-gnu-gcc.exe
set AR=%CROSS9%\x86_64-pc-linux-gnu-ar.exe

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

:: Create build directory if not exists
if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%"
)

:: Compilation options
set COMMON_FLAGS=-Wall -Wextra -fno-pie -fno-stack-protector -fno-omit-frame-pointer
set DEBUG_FLAGS=-g -O0 -DDEBUG
set CFLAGS=%COMMON_FLAGS% %DEBUG_FLAGS% -I "%INCLUDE_DIR%" -I "%COSMO%"

:: Linker options
set LDFLAGS=-static -nostdlib -Wl,-T,"%COSMO%\ape.lds" -Wl,--gc-sections -fuse-ld=bfd
set LIBS="%COSMO%\crt.o" "%COSMO%\ape.o" "%COSMO%\cosmopolitan.a"

:: Ensure library is built
if not exist "%BUILD_DIR%\libppdb.a" (
    echo Building library first...
    call "%SCRIPT_DIR%\build_ppdb.bat"
    if errorlevel 1 (
        echo Error building library
        exit /b 1
    )
)

:: Change to build directory for intermediate files
cd "%BUILD_DIR%"

:: Compile test framework
echo Compiling test framework...
"%GCC%" %CFLAGS% -c "%TEST_DIR%\test_framework.c"
if errorlevel 1 (
    echo Error compiling test framework
    exit /b 1
)

:: Compile test files
echo Compiling test files...
for %%f in ("%TEST_DIR%\test_*.c") do (
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
for %%f in ("%TEST_DIR%\test_*_main.c") do (
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
    "%GCC%" %LDFLAGS% "%%f" !test_name!.o test_framework.o "%BUILD_DIR%\libppdb.a" %LIBS% -o "%BUILD_DIR%\!test_name!.exe"
    
    :: Add APE self-modify support
    echo   Adding APE self-modify support to !test_name!...
    copy /b "%BUILD_DIR%\!test_name!.exe" + "%COSMO%\ape-copy-self.o" "%BUILD_DIR%\!test_name!.com"
    if errorlevel 1 (
        echo Error adding APE support to !test_name!
        exit /b 1
    )
)

:: Clean up intermediate files
echo Cleaning up...
del *.o

echo Test build completed successfully!
echo Test binaries in: %BUILD_DIR%
