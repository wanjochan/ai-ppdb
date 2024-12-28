@echo off
setlocal enabledelayedexpansion

REM ==================== Version Info ====================
set VERSION=1.0.0
set BUILD_TIME=%date% %time%

REM ==================== Command Line Args ====================
REM 检查参数
if "%1"=="" (
    echo Usage: build.bat [prod^|test] [lockfree^|locked]
    exit /b 1
)
if "%2"=="" (
    echo Usage: build.bat [prod^|test] [lockfree^|locked]
    exit /b 1
)

REM 设置构建类型
set BUILD_TYPE=%1
if not "%BUILD_TYPE%"=="prod" if not "%BUILD_TYPE%"=="test" (
    echo Error: Invalid build type '%BUILD_TYPE%'
    echo Usage: build.bat [prod^|test] [lockfree^|locked]
    exit /b 1
)

REM 设置版本标志
set BUILD_VERSION=%2
if not "%BUILD_VERSION%"=="lockfree" if not "%BUILD_VERSION%"=="locked" (
    echo Error: Invalid version '%BUILD_VERSION%'
    echo Usage: build.bat [prod^|test] [lockfree^|locked]
    exit /b 1
)

REM ==================== Environment Setup ====================
echo PPDB %BUILD_TYPE% Build v%VERSION% (%BUILD_TYPE%)
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

REM 清理旧的对象文件
if "%BUILD_TYPE%"=="test" (
    del /f /q "%BUILD_DIR%\test_*.o" 2>nul
)

REM 设置源文件
if "%BUILD_TYPE%"=="test" (
    if "%BUILD_VERSION%"=="lockfree" (
        set TEST_FILES=%ROOT_DIR%\test\lockfree\test_main.c %ROOT_DIR%\test\lockfree\test_atomic_skiplist.c %ROOT_DIR%\test\lockfree\test_atomic_wal.c
    ) else (
        set TEST_FILES=%ROOT_DIR%\test\lock\test_main.c %ROOT_DIR%\test\lock\test_kvstore.c %ROOT_DIR%\test\lock\test_memtable.c %ROOT_DIR%\test\lock\test_wal.c
    )
)

REM 设置源文件
if "%BUILD_VERSION%"=="lockfree" (
    set SRC_FILES=%ROOT_DIR%\src_lockfree\kvstore\atomic_skiplist.c %ROOT_DIR%\src_lockfree\kvstore\sharded_memtable.c %ROOT_DIR%\src_lockfree\kvstore\atomic_wal.c %ROOT_DIR%\src\main.c %ROOT_DIR%\src\common\logger.c %ROOT_DIR%\src\common\fs.c
) else (
    set SRC_FILES=%ROOT_DIR%\src\kvstore\skiplist.c %ROOT_DIR%\src\kvstore\memtable.c %ROOT_DIR%\src\kvstore\wal.c %ROOT_DIR%\src\main.c %ROOT_DIR%\src\common\logger.c %ROOT_DIR%\src\common\fs.c
)

REM ==================== Build Process ====================
echo Building source files...

REM Compile source files
for %%f in (%SRC_FILES%) do (
    echo   Compiling %%f...
    %CC% %CFLAGS% -c "%%f" -o "%BUILD_DIR%\%%~nf.o"
    if !errorlevel! neq 0 goto :error
)

if "%BUILD_TYPE%"=="test" (
    echo Building test files...
    for %%f in (%TEST_FILES%) do (
        echo   Compiling %%f...
        %CC% %CFLAGS% -c "%%f" -o "%BUILD_DIR%\%%~nf.o"
        if !errorlevel! neq 0 goto :error
    )
)

REM Create static library
echo Creating static library...
cd /d "%BUILD_DIR%"
if "%BUILD_VERSION%"=="lockfree" (
    "%ROOT_DIR%\cross9\bin\x86_64-pc-linux-gnu-ar.exe" rcs libppdb.a atomic_skiplist.o sharded_memtable.o atomic_wal.o logger.o fs.o
) else (
    "%ROOT_DIR%\cross9\bin\x86_64-pc-linux-gnu-ar.exe" rcs libppdb.a skiplist.o memtable.o wal.o logger.o fs.o
)
if !errorlevel! neq 0 goto :error

REM Link executable
echo Linking executable...
cd /d "%BUILD_DIR%"
if "%BUILD_TYPE%"=="prod" (
    if "%BUILD_VERSION%"=="lockfree" (
        %CC% %CFLAGS% %LDFLAGS% -o ppdb_lockfree.exe.dbg main.o libppdb.a "%COSMO_DIR%\crt.o" "%COSMO_DIR%\ape-no-modify-self.o" "%COSMO_DIR%\cosmopolitan.a"
        if !errorlevel! neq 0 goto :error
        %OBJCOPY% -S -O binary ppdb_lockfree.exe.dbg ppdb_lockfree.exe
        set PPDB_EXE=ppdb_lockfree.exe
    ) else (
        %CC% %CFLAGS% %LDFLAGS% -o ppdb_locked.exe.dbg main.o libppdb.a "%COSMO_DIR%\crt.o" "%COSMO_DIR%\ape-no-modify-self.o" "%COSMO_DIR%\cosmopolitan.a"
        if !errorlevel! neq 0 goto :error
        %OBJCOPY% -S -O binary ppdb_locked.exe.dbg ppdb_locked.exe
        set PPDB_EXE=ppdb_locked.exe
    )
) else (
    if "%BUILD_VERSION%"=="lockfree" (
        %CC% %CFLAGS% %LDFLAGS% -o ppdb_test_lockfree.exe.dbg test_main.o test_atomic_skiplist.o test_atomic_wal.o libppdb.a "%COSMO_DIR%\crt.o" "%COSMO_DIR%\ape-no-modify-self.o" "%COSMO_DIR%\cosmopolitan.a"
        if !errorlevel! neq 0 goto :error
        %OBJCOPY% -S -O binary ppdb_test_lockfree.exe.dbg ppdb_test_lockfree.exe
        set PPDB_EXE=ppdb_test_lockfree.exe
    ) else (
        %CC% %CFLAGS% %LDFLAGS% -o ppdb_test_locked.exe.dbg test_main.o test_kvstore.o test_memtable.o test_wal.o libppdb.a "%COSMO_DIR%\crt.o" "%COSMO_DIR%\ape-no-modify-self.o" "%COSMO_DIR%\cosmopolitan.a"
        if !errorlevel! neq 0 goto :error
        %OBJCOPY% -S -O binary ppdb_test_locked.exe.dbg ppdb_test_locked.exe
        set PPDB_EXE=ppdb_test_locked.exe
    )
)

echo Build completed successfully

echo.
if "%BUILD_TYPE%"=="test" (
    echo Running tests...
) else (
    echo Running program...
)
.\%PPDB_EXE%
if !errorlevel! neq 0 (
    if "%BUILD_TYPE%"=="test" (
        echo Error: Tests failed
    ) else (
        echo Error: Program exited with error
    )
    cd /d "%ROOT_DIR%"
    exit /b 1
)

cd /d "%ROOT_DIR%"
if "%BUILD_TYPE%"=="test" (
    echo All tests passed successfully
) else (
    echo Program ran successfully
)
exit /b 0

:error
echo Error: Build failed
cd /d "%ROOT_DIR%"
exit /b 1 