@echo off
setlocal EnableDelayedExpansion

rem Get build mode from parameter
set "BUILD_MODE=%1"
if "%BUILD_MODE%"=="" set "BUILD_MODE=release"

rem Load environment variables and common functions
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

rem Create build directory if it doesn't exist
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo Building base tests...

REM Build and run memory test
echo Building memory test...
"%GCC%" %CFLAGS% "%PPDB_DIR%\src\base.c" "%PPDB_DIR%\test\white\infra\test_memory.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\memory_test.exe.dbg"
if errorlevel 1 exit /b 1
"%OBJCOPY%" -S -O binary "%BUILD_DIR%\memory_test.exe.dbg" "%BUILD_DIR%\memory_test.exe"
if errorlevel 1 exit /b 1
echo Running memory test...
"%BUILD_DIR%\memory_test.exe"
if errorlevel 1 exit /b 1

REM Build and run error test
echo Building error test...
"%GCC%" %CFLAGS% "%PPDB_DIR%\src\base.c" "%PPDB_DIR%\test\white\infra\test_error.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\error_test.exe.dbg"
if errorlevel 1 exit /b 1
"%OBJCOPY%" -S -O binary "%BUILD_DIR%\error_test.exe.dbg" "%BUILD_DIR%\error_test.exe"
if errorlevel 1 exit /b 1
echo Running error test...
"%BUILD_DIR%\error_test.exe"
if errorlevel 1 exit /b 1

REM Build and run log test
echo Building log test...
"%GCC%" %CFLAGS% "%PPDB_DIR%\src\base.c" "%PPDB_DIR%\test\white\infra\test_log.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\log_test.exe.dbg"
if errorlevel 1 exit /b 1
"%OBJCOPY%" -S -O binary "%BUILD_DIR%\log_test.exe.dbg" "%BUILD_DIR%\log_test.exe"
if errorlevel 1 exit /b 1
echo Running log test...
"%BUILD_DIR%\log_test.exe"
if errorlevel 1 exit /b 1

REM Build and run sync test
echo Building sync test...
"%GCC%" %CFLAGS% "%PPDB_DIR%\src\base.c" "%PPDB_DIR%\src\base\sync.c" "%PPDB_DIR%\test\white\infra\test_sync.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\sync_test.exe.dbg"
if errorlevel 1 exit /b 1
"%OBJCOPY%" -S -O binary "%BUILD_DIR%\sync_test.exe.dbg" "%BUILD_DIR%\sync_test.exe"
if errorlevel 1 exit /b 1
echo Running sync test...
"%BUILD_DIR%\sync_test.exe"
if errorlevel 1 exit /b 1

REM Build and run skiplist test
echo Building skiplist test...
"%GCC%" %CFLAGS% "%PPDB_DIR%\src\base.c" "%PPDB_DIR%\src\base\skiplist.c" "%PPDB_DIR%\test\white\infra\test_skiplist.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\skiplist_test.exe.dbg"
if errorlevel 1 exit /b 1
"%OBJCOPY%" -S -O binary "%BUILD_DIR%\skiplist_test.exe.dbg" "%BUILD_DIR%\skiplist_test.exe"
if errorlevel 1 exit /b 1
echo Running skiplist test...
"%BUILD_DIR%\skiplist_test.exe"
if errorlevel 1 exit /b 1

REM Build and run btree test
echo Building btree test...
"%GCC%" %CFLAGS% "%PPDB_DIR%\src\base.c" "%PPDB_DIR%\src\base\btree.c" "%PPDB_DIR%\test\white\infra\test_btree.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\btree_test.exe.dbg"
if errorlevel 1 exit /b 1
"%OBJCOPY%" -S -O binary "%BUILD_DIR%\btree_test.exe.dbg" "%BUILD_DIR%\btree_test.exe"
if errorlevel 1 exit /b 1
echo Running btree test...
"%BUILD_DIR%\btree_test.exe"
if errorlevel 1 exit /b 1

echo All base tests passed!