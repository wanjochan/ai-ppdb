@echo off

call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

echo Building infra library first...
call "%~dp0\build_infra.bat"
if errorlevel 1 exit /b 1

echo Building test framework...
if not exist "%BUILD_DIR%\test\white\framework" mkdir "%BUILD_DIR%\test\white\framework"
"%GCC%" %CFLAGS% -I"%SRC_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" "%PPDB_DIR%\test\white\framework\test_framework.c" -c -o "%BUILD_DIR%\test\white\framework\test_framework.o"
if errorlevel 1 exit /b 1

echo Building mock framework...
"%GCC%" %CFLAGS% -I"%SRC_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" "%PPDB_DIR%\test\white\framework\mock_framework.c" -c -o "%BUILD_DIR%\test\white\framework\mock_framework.o"
if errorlevel 1 exit /b 1

echo Building test cases...
set TEST_FILES=test_memory.c test_log.c test_sync.c test_error.c test_metrics.c test_skiplist.c test_struct.c test_peer.c test_memtable.c

for %%f in (%TEST_FILES%) do (
    echo Building %%f...
    "%GCC%" %CFLAGS% -I"%SRC_DIR%" -I"%TEST_DIR%" -I"%TEST_DIR%\white" "%PPDB_DIR%\test\white\infra\%%f" -c -o "%BUILD_DIR%\test\white\infra\%%~nf.o"
    if errorlevel 1 exit /b 1

    echo Linking %%~nf...
    "%GCC%" "%BUILD_DIR%\test\white\framework\test_framework.o" "%BUILD_DIR%\test\white\framework\mock_framework.o" "%BUILD_DIR%\test\white\infra\%%~nf.o" "%BUILD_DIR%\infra\libinfra.a" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\test\white\infra\%%~nf.exe.dbg"
    if errorlevel 1 exit /b 1

    "%OBJCOPY%" -S -O binary "%BUILD_DIR%\test\white\infra\%%~nf.exe.dbg" "%BUILD_DIR%\test\white\infra\%%~nf.exe"
    if errorlevel 1 exit /b 1

    if not "%1"=="norun" (
        echo Running %%~nf...
        "%BUILD_DIR%\test\white\infra\%%~nf.exe"
    )
)

echo Build complete. 
