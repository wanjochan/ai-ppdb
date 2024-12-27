@echo off
setlocal enabledelayedexpansion

REM 设置编译环境
call "%~dp0common.bat"

REM 设置目录
set BUILD_DIR=%~dp0..\build\test
set TEST_DIR=%~dp0..\test_white
set SRC_DIR=%~dp0..\src
set INCLUDE_DIR=%~dp0..\include

REM 创建构建目录
if not exist %BUILD_DIR% mkdir %BUILD_DIR%

REM 编译选项
set CFLAGS=/nologo /W3 /WX- /O2 /Oi /GL /D "WIN32" /D "_WINDOWS" /D "_CRT_SECURE_NO_WARNINGS" /Gm- /EHsc /MT /GS /Gy /fp:precise /Zc:wchar_t /Zc:forScope /Gd /TC
set INCLUDES=/I"%INCLUDE_DIR%" /I"%TEST_DIR%" /I"%COSMO_DIR%\include"
set LIBS=user32.lib gdi32.lib shell32.lib ole32.lib

REM 编译WAL测试
echo Building WAL test...
cl %CFLAGS% %INCLUDES% /Fe:"%BUILD_DIR%\test_wal.exe" ^
    "%TEST_DIR%\test_wal_main.c" ^
    "%TEST_DIR%\test_wal.c" ^
    "%TEST_DIR%\test_framework.c" ^
    "%SRC_DIR%\kvstore\wal.c" ^
    "%SRC_DIR%\common\fs.c" ^
    "%SRC_DIR%\common\logger.c" ^
    "%COSMO_DIR%\lib\ape.lib" ^
    %LIBS%

if %ERRORLEVEL% neq 0 (
    echo Build failed with error code %ERRORLEVEL%
    echo Current directory: %CD%
    echo Build directory: %BUILD_DIR%
    exit /b %ERRORLEVEL%
)

REM 编译MemTable测试
echo Building MemTable test...
cl %CFLAGS% %INCLUDES% /Fe:"%BUILD_DIR%\test_memtable.exe" ^
    "%TEST_DIR%\test_memtable_main.c" ^
    "%TEST_DIR%\test_memtable.c" ^
    "%TEST_DIR%\test_framework.c" ^
    "%SRC_DIR%\kvstore\memtable.c" ^
    "%SRC_DIR%\kvstore\skiplist.c" ^
    "%SRC_DIR%\common\logger.c" ^
    "%COSMO_DIR%\lib\ape.lib" ^
    %LIBS%

if %ERRORLEVEL% neq 0 (
    echo Build failed with error code %ERRORLEVEL%
    echo Current directory: %CD%
    echo Build directory: %BUILD_DIR%
    exit /b %ERRORLEVEL%
)

REM 编译KVStore测试
echo Building KVStore test...
cl %CFLAGS% %INCLUDES% /Fe:"%BUILD_DIR%\test_kvstore.exe" ^
    "%TEST_DIR%\test_kvstore_main.c" ^
    "%TEST_DIR%\test_kvstore.c" ^
    "%TEST_DIR%\test_framework.c" ^
    "%SRC_DIR%\kvstore\kvstore.c" ^
    "%SRC_DIR%\kvstore\memtable.c" ^
    "%SRC_DIR%\kvstore\skiplist.c" ^
    "%SRC_DIR%\kvstore\wal.c" ^
    "%SRC_DIR%\common\fs.c" ^
    "%SRC_DIR%\common\logger.c" ^
    "%COSMO_DIR%\lib\ape.lib" ^
    %LIBS%

if %ERRORLEVEL% neq 0 (
    echo Build failed with error code %ERRORLEVEL%
    echo Current directory: %CD%
    echo Build directory: %BUILD_DIR%
    exit /b %ERRORLEVEL%
)

REM 运行WAL测试
echo Running WAL test...
"%BUILD_DIR%\test_wal.exe"

if %ERRORLEVEL% neq 0 (
    echo WAL tests failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

REM 运行MemTable测试
echo Running MemTable test...
"%BUILD_DIR%\test_memtable.exe"

if %ERRORLEVEL% neq 0 (
    echo MemTable tests failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

REM 运行KVStore测试
echo Running KVStore test...
"%BUILD_DIR%\test_kvstore.exe"

if %ERRORLEVEL% neq 0 (
    echo KVStore tests failed with error code %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo All tests completed successfully
exit /b 0 
