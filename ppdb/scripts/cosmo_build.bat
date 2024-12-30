@echo off
setlocal EnableDelayedExpansion

rem Get parameters
set "OUT_FILE=%~1"
set "SOURCES=%~2"
set "EXTRA_FLAGS=%~3"

echo.
echo ===== Build Configuration =====
echo Output file: %OUT_FILE%
echo Source files: [%SOURCES%]
echo Extra flags: [%EXTRA_FLAGS%]
echo.

rem Set paths
set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%\.."
set "ROOT_DIR=%CD%"
set "BUILD_DIR=%ROOT_DIR%\build"
set "INCLUDE_DIR=%ROOT_DIR%\include"
set "COSMO=%ROOT_DIR%\..\cosmopolitan"
set "CROSS9=%ROOT_DIR%\..\cross9\bin"
set "GCC=%CROSS9%\x86_64-pc-linux-gnu-gcc.exe"
set "AR=%CROSS9%\x86_64-pc-linux-gnu-ar.exe"
set "OBJCOPY=%CROSS9%\x86_64-pc-linux-gnu-objcopy.exe"

rem Check directories and tools
if not exist "%COSMO%" (
    echo Error: Cosmopolitan directory not found
    exit /b 1
)

if not exist "%CROSS9%" (
    echo Error: Cross9 directory not found
    exit /b 1
)

if not exist "%GCC%" (
    echo Error: GCC not found
    exit /b 1
)

if not exist "%AR%" (
    echo Error: AR not found
    exit /b 1
)

if not exist "%OBJCOPY%" (
    echo Error: OBJCOPY not found
    exit /b 1
)

rem Create build directory if not exists
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem Set compilation flags
set "COMMON_FLAGS=-Wall -Wextra -fno-pie -fno-stack-protector -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -fno-asynchronous-unwind-tables"
set "DEBUG_FLAGS=-g -O0 -DDEBUG"
set "CFLAGS=%COMMON_FLAGS% %DEBUG_FLAGS% -nostdinc -I%ROOT_DIR% -I%ROOT_DIR%\include -I%ROOT_DIR%\src -I%COSMO% -include %COSMO%\cosmopolitan.h %EXTRA_FLAGS%"
set "LDFLAGS=-static -nostdlib -Wl,-T,%COSMO%\ape.lds -Wl,--gc-sections -fuse-ld=bfd -Wl,-z,max-page-size=0x1000 -no-pie"
set "LIBS=%COSMO%\crt.o %COSMO%\ape.o %COSMO%\cosmopolitan.a"

echo ===== Compilation Flags =====
echo CFLAGS: %CFLAGS%
echo LDFLAGS: %LDFLAGS%
echo LIBS: %LIBS%
echo.

rem Compile each source file to object file
set "OBJ_FILES="
for %%F in (%SOURCES%) do (
    set "OBJ_FILE=%BUILD_DIR%\%%~nF.o"
    echo Compiling %%F to !OBJ_FILE!...
    "%GCC%" %CFLAGS% -c "%%F" -o "!OBJ_FILE!"
    if errorlevel 1 (
        echo Error: Failed to compile %%F
        exit /b 1
    )
    set "OBJ_FILES=!OBJ_FILES! !OBJ_FILE!"
)

echo.
echo ===== Linking =====
echo Object files: [%OBJ_FILES%]
echo.

rem Link object files
echo Linking...
"%GCC%" %LDFLAGS% %OBJ_FILES% %LIBS% -o "%BUILD_DIR%\%OUT_FILE%.dbg"
if errorlevel 1 (
    echo Error: Linking failed
    exit /b 1
)

rem Convert to APE format
echo Converting to APE format...
"%OBJCOPY%" -S -O binary "%BUILD_DIR%\%OUT_FILE%.dbg" "%BUILD_DIR%\%OUT_FILE%.exe"
if errorlevel 1 (
    echo Error: objcopy failed
    exit /b 1
)

echo Build completed successfully
endlocal & exit /b 0 