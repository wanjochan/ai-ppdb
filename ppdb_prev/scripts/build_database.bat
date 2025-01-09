@echo off
setlocal EnableDelayedExpansion

rem Load environment variables
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

echo Building database layer...

rem Build database layer
%CC% %CFLAGS% -c ^
%SRC_DIR%\database\database.c ^
%SRC_DIR%\database\database_txn.c ^
%SRC_DIR%\database\database_mvcc.c ^
%SRC_DIR%\database\database_memkv.c ^
%SRC_DIR%\database\database_index.c ^
-o %BUILD_DIR%\database.o
if errorlevel 1 exit /b 1

rem Run tests if not disabled
if not "%1"=="notest" (
    call "%~dp0\build_test.bat"
    if errorlevel 1 exit /b 1
    echo Running database tests...
    %BUILD_DIR%\test.exe database
    if errorlevel 1 exit /b 1
)

echo Database layer build completed 