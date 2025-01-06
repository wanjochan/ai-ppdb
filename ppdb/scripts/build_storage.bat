@echo off
setlocal enabledelayedexpansion

REM Set paths
set ROOT_DIR=%~dp0..
set SRC_DIR=%ROOT_DIR%\src
set TEST_DIR=%ROOT_DIR%\test\white\storage
set BUILD_DIR=%ROOT_DIR%\build
set BIN_DIR=%BUILD_DIR%\bin
set OBJ_DIR=%BUILD_DIR%\obj

REM Create directories if they don't exist
if not exist %BUILD_DIR% mkdir %BUILD_DIR%
if not exist %BIN_DIR% mkdir %BIN_DIR%
if not exist %OBJ_DIR% mkdir %OBJ_DIR%

REM Set compiler flags
set CFLAGS=-O2 -g -Wall -Wextra -I%SRC_DIR% -I%ROOT_DIR%\include

REM Build storage module
echo Building storage module...
gcc %CFLAGS% -c %SRC_DIR%\storage\*.c -o %OBJ_DIR%\storage.o

REM Build and run tests
echo Building and running storage tests...

REM Build and run initialization tests
echo Running storage initialization tests...
gcc %CFLAGS% %TEST_DIR%\test_storage_init.c %OBJ_DIR%\storage.o -o %BIN_DIR%\test_storage_init
%BIN_DIR%\test_storage_init
if errorlevel 1 goto :error

REM Build and run table tests
echo Running storage table tests...
gcc %CFLAGS% %TEST_DIR%\test_storage_table.c %OBJ_DIR%\storage.o -o %BIN_DIR%\test_storage_table
%BIN_DIR%\test_storage_table
if errorlevel 1 goto :error

REM Build and run index tests
echo Running storage index tests...
gcc %CFLAGS% %TEST_DIR%\test_storage_index.c %OBJ_DIR%\storage.o -o %BIN_DIR%\test_storage_index
%BIN_DIR%\test_storage_index
if errorlevel 1 goto :error

REM Build and run data tests
echo Running storage data tests...
gcc %CFLAGS% %TEST_DIR%\test_storage_data.c %OBJ_DIR%\storage.o -o %BIN_DIR%\test_storage_data
%BIN_DIR%\test_storage_data
if errorlevel 1 goto :error

REM Build and run maintenance tests
echo Running storage maintenance tests...
gcc %CFLAGS% %TEST_DIR%\test_storage_maintain.c %OBJ_DIR%\storage.o -o %BIN_DIR%\test_storage_maintain
%BIN_DIR%\test_storage_maintain
if errorlevel 1 goto :error

echo All storage tests completed successfully
exit /b 0

:error
echo Error occurred during build or test
exit /b 1