@echo off
setlocal EnableDelayedExpansion

set "TARGET=%1"
set "BUILD_MODE=%2"

if "%TARGET%"=="" set "TARGET=help"
if "%BUILD_MODE%"=="" set "BUILD_MODE=release"

call "%~dp0\build_%TARGET%.bat" %BUILD_MODE%
exit /b !errorlevel!
