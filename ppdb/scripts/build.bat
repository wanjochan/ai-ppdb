@echo off
setlocal enabledelayedexpansion

REM ==================== Version Info ====================
set VERSION=1.0.0
set BUILD_TIME=%date% %time%

REM ==================== Environment Setup ====================
echo PPDB Build v%VERSION%
echo Build time: %BUILD_TIME%
echo.

REM Set base directories
set ROOT_DIR=%~dp0..
set BUILD_DIR=%ROOT_DIR%\build
set COSMO_DIR=%ROOT_DIR%\cosmopolitan
set CC=%ROOT_DIR%\cross9\bin\x86_64-pc-linux-gnu-gcc.exe
set OBJCOPY=%ROOT_DIR%\cross9\bin\x86_64-pc-linux-gnu-objcopy.exe

REM Set compiler flags
set CFLAGS=-g -O2 -pipe -Wall -Wextra -I%ROOT_DIR%\include -I%COSMO_DIR% -fno-pie -no-pie -mno-red-zone -fno-omit-frame-pointer -nostdlib -nostdinc -fPIC
set LDFLAGS=-static -nostdlib -Wl,--gc-sections -Wl,-z,max-page-size=0x1000 -fuse-ld=bfd -Wl,-T,"%COSMO_DIR%\ape.lds"

REM 创建构建目录
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

REM 设置源文件
set SRC_FILES=%ROOT_DIR%\src\kvstore\skiplist\skiplist.c %ROOT_DIR%\src\kvstore\skiplist\atomic_skiplist.c ^
%ROOT_DIR%\src\kvstore\memtable\memtable.c %ROOT_DIR%\src\kvstore\memtable\atomic_memtable.c %ROOT_DIR%\src\kvstore\memtable\sharded_memtable.c ^
%ROOT_DIR%\src\kvstore\wal\wal.c %ROOT_DIR%\src\kvstore\wal\atomic_wal.c %ROOT_DIR%\src\kvstore\kvstore.c ^
%ROOT_DIR%\src\common\logger.c %ROOT_DIR%\src\common\fs.c %ROOT_DIR%\src\common\ref_count\ref_count.c ^
%ROOT_DIR%\src\main.c

REM 设置测试文件
set TEST_FILES=%ROOT_DIR%\test\white\test_main.c %ROOT_DIR%\test\white\test_framework.c ^
%ROOT_DIR%\test\white\test_kvstore.c %ROOT_DIR%\test\white\test_memtable.c %ROOT_DIR%\test\white\test_wal.c

REM ==================== Build Process ====================
echo Building source files...

REM Compile source files
for %%f in (%SRC_FILES%) do (
    echo   Compiling %%f...
    %CC% %CFLAGS% -c "%%f" -o "%BUILD_DIR%\%%~nf.o"
    if !errorlevel! neq 0 goto :error
)

REM Create static library
echo Creating static library...
cd /d "%BUILD_DIR%"
"%ROOT_DIR%\cross9\bin\x86_64-pc-linux-gnu-ar.exe" rcs libppdb.a skiplist.o atomic_skiplist.o memtable.o atomic_memtable.o sharded_memtable.o wal.o atomic_wal.o kvstore.o logger.o fs.o ref_count.o

REM Link executable
echo Linking executable...
cd /d "%BUILD_DIR%"
%CC% %CFLAGS% %LDFLAGS% -o ppdb.exe.dbg main.o libppdb.a "%COSMO_DIR%\crt.o" "%COSMO_DIR%\ape-no-modify-self.o" "%COSMO_DIR%\cosmopolitan.a"
if !errorlevel! neq 0 goto :error
%OBJCOPY% -S -O binary ppdb.exe.dbg ppdb.exe

REM Build and run tests
echo Building test files...
for %%f in (%TEST_FILES%) do (
    echo   Compiling %%f...
    %CC% %CFLAGS% -c "%%f" -o "%BUILD_DIR%\%%~nf.o"
    if !errorlevel! neq 0 goto :error
)

echo Linking test executable...
cd /d "%BUILD_DIR%"
%CC% %CFLAGS% %LDFLAGS% -o ppdb_test.exe.dbg test_main.o test_framework.o test_kvstore.o test_memtable.o test_wal.o libppdb.a "%COSMO_DIR%\crt.o" "%COSMO_DIR%\ape-no-modify-self.o" "%COSMO_DIR%\cosmopolitan.a"
if !errorlevel! neq 0 goto :error
%OBJCOPY% -S -O binary ppdb_test.exe.dbg ppdb_test.exe

echo Build completed successfully

echo.
echo Running tests...
.\ppdb_test.exe
if !errorlevel! neq 0 (
    echo Error: Tests failed
    cd /d "%ROOT_DIR%"
    exit /b 1
)

echo All tests passed successfully

echo.
echo Running program...
.\ppdb.exe
if !errorlevel! neq 0 (
    echo Error: Program exited with error
    cd /d "%ROOT_DIR%"
    exit /b 1
)

cd /d "%ROOT_DIR%"
echo Program ran successfully
exit /b 0

:error
echo Error: Build failed
cd /d "%ROOT_DIR%"
exit /b 1 