@echo off
rem @cursor:protected
rem This file is considered semi-read-only by Cursor AI.
rem Any modifications should be discussed and confirmed before applying.

setlocal EnableDelayedExpansion

set "TARGET=%1"
set "BUILD_MODE=%2"

if "%TARGET%"=="" set "TARGET=help"
if "%BUILD_MODE%"=="" set "BUILD_MODE=release"

call "%~dp0\build_%TARGET%.bat" %BUILD_MODE%
exit /b !errorlevel!
