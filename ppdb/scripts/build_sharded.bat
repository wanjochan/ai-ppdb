@echo off
setlocal

rem Set environment variables
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b %errorlevel%

rem Build sharded memtable test
echo Building sharded memtable test...
%GCC% %CFLAGS% ^
"%PPDB_DIR%\src\base.c" ^
"%PPDB_DIR%\src\database.c" ^
"%PPDB_DIR%\test\white\test_framework.c" ^
"%PPDB_DIR%\test\white\database\test_database_sharded.c" ^
%LDFLAGS% %LIBS% -o "%BUILD_DIR%\test_sharded.exe.dbg"

rem Strip debug symbols
"%OBJCOPY%" -S -O binary "%BUILD_DIR%\test_sharded.exe.dbg" "%BUILD_DIR%\test_sharded.exe"

rem Run sharded memtable test
echo Running sharded memtable test...
"%BUILD_DIR%\test_sharded.exe"
if errorlevel 1 (
    echo Sharded memtable test failed with error code !errorlevel!
    exit /b !errorlevel!
)
echo Sharded memtable test passed!
exit /b 0 