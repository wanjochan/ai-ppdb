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

echo Building engine tests...

REM Build and run transaction test
echo Building transaction test...
%GCC% %CFLAGS% "%PPDB_DIR%\src\base.c" "%PPDB_DIR%\src\engine.c" "%PPDB_DIR%\test\white\engine\test_txn.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\txn_test.exe.dbg"
if errorlevel 1 exit /b 1
"%OBJCOPY%" -S -O binary "%BUILD_DIR%\txn_test.exe.dbg" "%BUILD_DIR%\txn_test.exe"
if errorlevel 1 exit /b 1

echo Running transaction test...
"%BUILD_DIR%\txn_test.exe"
if errorlevel 1 exit /b 1

REM Build and run IO test
echo Building IO test...
%GCC% %CFLAGS% "%PPDB_DIR%\src\base.c" "%PPDB_DIR%\src\engine.c" "%PPDB_DIR%\test\white\engine\test_io.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\io_test.exe.dbg"
if errorlevel 1 exit /b 1
"%OBJCOPY%" -S -O binary "%BUILD_DIR%\io_test.exe.dbg" "%BUILD_DIR%\io_test.exe"
if errorlevel 1 exit /b 1

echo Running IO test...
"%BUILD_DIR%\io_test.exe"
if errorlevel 1 exit /b 1

REM Build and run core test
echo Building core test...
%GCC% %CFLAGS% "%PPDB_DIR%\src\base.c" "%PPDB_DIR%\src\engine.c" "%PPDB_DIR%\test\white\engine\test_core.c" %LDFLAGS% %LIBS% -o "%BUILD_DIR%\core_test.exe.dbg"
if errorlevel 1 exit /b 1
"%OBJCOPY%" -S -O binary "%BUILD_DIR%\core_test.exe.dbg" "%BUILD_DIR%\core_test.exe"
if errorlevel 1 exit /b 1

echo Running core test...
"%BUILD_DIR%\core_test.exe"
if errorlevel 1 exit /b 1

echo All engine tests passed!
