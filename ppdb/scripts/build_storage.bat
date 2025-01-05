@echo off
setlocal enabledelayedexpansion

REM Build storage tests
echo Building storage tests...

REM Build base library first
echo Building base library...
call build_base.bat
if errorlevel 1 goto error

REM Build engine library next
echo Building engine library...
call build_engine.bat
if errorlevel 1 goto error

REM Build storage library
echo Building storage library...
cosmocc -o ..\build\storage.o -c ..\src\storage.c -I..\src -I..\include
if errorlevel 1 goto error

REM Build storage test
echo Building storage test...
cosmocc -o ..\build\test_storage.exe ..\test\white\storage\test_storage.c ..\build\storage.o ..\build\engine.o ..\build\base.o -I..\src -I..\include -I..\test\white
if errorlevel 1 goto error

REM Run storage test
echo Running storage test...
..\build\test_storage.exe
if errorlevel 1 goto error

echo All storage tests passed
exit /b 0

:error
echo Build failed
exit /b 1 