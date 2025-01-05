@echo off
setlocal EnableDelayedExpansion

rem ===== Get Build Mode =====
set "BUILD_MODE=%1"
if "%BUILD_MODE%"=="" set "BUILD_MODE=release"

rem ===== Load Common Environment =====
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

echo ===== Running Core Layer Tests =====

rem ===== Run Transaction Tests =====
echo.
echo [1/4] Testing transaction management...
"%GCC%" %CFLAGS% -I"%PPDB_DIR%\include" ^
    "%PPDB_DIR%\test\white\core\test_transaction.c" ^
    "%PPDB_DIR%\src\base.c" ^
    "%PPDB_DIR%\src\core.c" ^
    -o "%BUILD_DIR%\test_transaction.exe"
if errorlevel 1 exit /b 1

"%BUILD_DIR%\test_transaction.exe"
if errorlevel 1 exit /b 1

rem ===== Run Concurrency Tests =====
echo.
echo [2/4] Testing concurrency control...
"%GCC%" %CFLAGS% -I"%PPDB_DIR%\include" ^
    "%PPDB_DIR%\test\white\core\test_concurrency.c" ^
    "%PPDB_DIR%\src\base.c" ^
    "%PPDB_DIR%\src\core.c" ^
    -o "%BUILD_DIR%\test_concurrency.exe"
if errorlevel 1 exit /b 1

"%BUILD_DIR%\test_concurrency.exe"
if errorlevel 1 exit /b 1

rem ===== Run MVCC Tests =====
echo.
echo [3/4] Testing MVCC...
"%GCC%" %CFLAGS% -I"%PPDB_DIR%\include" ^
    "%PPDB_DIR%\test\white\core\test_mvcc.c" ^
    "%PPDB_DIR%\src\base.c" ^
    "%PPDB_DIR%\src\core.c" ^
    -o "%BUILD_DIR%\test_mvcc.exe"
if errorlevel 1 exit /b 1

"%BUILD_DIR%\test_mvcc.exe"
if errorlevel 1 exit /b 1

rem ===== Run Storage Interface Tests =====
echo.
echo [4/4] Testing storage interface...
"%GCC%" %CFLAGS% -I"%PPDB_DIR%\include" ^
    "%PPDB_DIR%\test\white\core\test_storage_interface.c" ^
    "%PPDB_DIR%\src\base.c" ^
    "%PPDB_DIR%\src\core.c" ^
    -o "%BUILD_DIR%\test_storage_interface.exe"
if errorlevel 1 exit /b 1

"%BUILD_DIR%\test_storage_interface.exe"
if errorlevel 1 exit /b 1

echo.
echo ===== All Core Layer Tests Passed =====
exit /b 0
