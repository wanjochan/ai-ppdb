@echo off
setlocal

REM Load common configuration
call "%~dp0\common.bat"

REM Set library sources
set "LIB_SOURCES=%ROOT_DIR%src/kvstore/kvstore.c %ROOT_DIR%src/kvstore/memtable.c %ROOT_DIR%src/kvstore/wal.c"

REM Build library
call :compile "ppdb" "%LIB_SOURCES%"
exit /b %errorlevel%
