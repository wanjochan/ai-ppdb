@echo off
setlocal

rem Set environment variables
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b %errorlevel%

rem Build memtable test
echo Building memtable test...
%GCC% %CFLAGS% ^
"%PPDB_DIR%\src\base.c" ^
"%PPDB_DIR%\src\database.c" ^
"%PPDB_DIR%\test\white\test_framework.c" ^
"%PPDB_DIR%\test\white\database\test_database_memtable.c" ^
%LDFLAGS% %LIBS% -o "%BUILD_DIR%\test_memtable.exe.dbg"

rem Strip debug symbols
"%OBJCOPY%" -S -O binary "%BUILD_DIR%\test_memtable.exe.dbg" "%BUILD_DIR%\test_memtable.exe"

rem Run memtable test
echo Running memtable test...
"%BUILD_DIR%\test_memtable.exe"
if errorlevel 1 (
    echo Memtable test failed with error code !errorlevel!
    exit /b !errorlevel!
)
echo Memtable test passed!
exit /b 0 