@echo off
setlocal

rem Set environment variables
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b %errorlevel%

rem Build kvstore test
echo Building kvstore test...
%GCC% %CFLAGS% ^
"%PPDB_DIR%\src\base.c" ^
"%PPDB_DIR%\src\database.c" ^
"%PPDB_DIR%\test\white\test_framework.c" ^
"%PPDB_DIR%\test\white\database\test_database_kvstore.c" ^
%LDFLAGS% %LIBS% -o "%BUILD_DIR%\test_kvstore.exe.dbg"

rem Strip debug symbols
"%OBJCOPY%" -S -O binary "%BUILD_DIR%\test_kvstore.exe.dbg" "%BUILD_DIR%\test_kvstore.exe"

rem Run kvstore test
echo Running kvstore test...
"%BUILD_DIR%\test_kvstore.exe"
if errorlevel 1 (
    echo Kvstore test failed with error code !errorlevel!
    exit /b !errorlevel!
)
echo Kvstore test passed!
exit /b 0 