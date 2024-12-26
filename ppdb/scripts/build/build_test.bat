@echo off
setlocal

REM Load common configuration
call "%~dp0\common.bat"

REM Set test sources
set "TEST_SOURCES=%ROOT_DIR%src/kvstore/test.c %ROOT_DIR%src/kvstore/kvstore.c"

REM Build test program
call :compile "ppdb_test" "%TEST_SOURCES%"
exit /b %errorlevel%
