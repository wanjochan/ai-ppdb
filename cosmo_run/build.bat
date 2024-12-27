@echo off
echo Building dynamic compiler...

:: Setup Visual Studio environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

set COSMO_PATH=..\ppdb\cosmopolitan
set INCLUDE_PATH=/I. /I%COSMO_PATH%

:: Clean old files
del /Q *.obj *.exe 2>nul

:: Compile source files
cl /nologo /W3 /O2 %INCLUDE_PATH% /c dynamic_compiler.c
if errorlevel 1 goto error

cl /nologo /W3 /O2 %INCLUDE_PATH% /c test_compiler.c
if errorlevel 1 goto error

:: Link
link /nologo dynamic_compiler.obj test_compiler.obj %COSMO_PATH%\ape.o /out:test_compiler.exe
if errorlevel 1 goto error

echo Build successful
exit /b 0

:error
echo Build failed
exit /b 1
