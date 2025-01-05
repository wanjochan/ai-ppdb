@echo off
setlocal EnableDelayedExpansion

rem ===== Get Build Mode =====
set "BUILD_MODE=%1"
if "%BUILD_MODE%"=="" set "BUILD_MODE=release"

rem ===== Load Common Environment =====
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

echo ===== Running Base Layer Tests =====

rem ===== Run Sync Tests =====
echo.
echo Testing synchronization primitives...
echo [1/6] Testing locked sync...
call "%~dp0\build_sync.bat" locked %BUILD_MODE%
if errorlevel 1 exit /b 1

echo [2/6] Testing lockfree sync...
call "%~dp0\build_sync.bat" lockfree %BUILD_MODE%
if errorlevel 1 exit /b 1

rem ===== Run Skiplist Tests =====
echo.
echo [3/6] Testing skiplist...
call "%~dp0\build_skiplist.bat" %BUILD_MODE%
if errorlevel 1 exit /b 1

rem ===== Run Log Tests =====
echo.
echo [4/6] Testing logging system...
"%GCC%" %CFLAGS% -I"%PPDB_DIR%\include" ^
    "%PPDB_DIR%\test\white\infra\test_log.c" ^
    "%PPDB_DIR%\src\base.c" ^
    -o "%BUILD_DIR%\test_log.exe"
if errorlevel 1 exit /b 1

"%BUILD_DIR%\test_log.exe"
if errorlevel 1 exit /b 1

rem ===== Run Memory Tests =====
echo.
echo [5/6] Testing memory management...
"%GCC%" %CFLAGS% -I"%PPDB_DIR%\include" ^
    "%PPDB_DIR%\test\white\infra\test_memory.c" ^
    "%PPDB_DIR%\src\base.c" ^
    -o "%BUILD_DIR%\test_memory.exe"
if errorlevel 1 exit /b 1

"%BUILD_DIR%\test_memory.exe"
if errorlevel 1 exit /b 1

rem ===== Run Error Handling Tests =====
echo.
echo [6/6] Testing error handling...
"%GCC%" %CFLAGS% -I"%PPDB_DIR%\include" ^
    "%PPDB_DIR%\test\white\infra\test_error.c" ^
    "%PPDB_DIR%\src\base.c" ^
    -o "%BUILD_DIR%\test_error.exe"
if errorlevel 1 exit /b 1

"%BUILD_DIR%\test_error.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== All Base Layer Tests Passed =====
exit /b 0 