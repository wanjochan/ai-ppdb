@echo off
setlocal EnableDelayedExpansion

rem Load environment variables and common functions
call "%~dp0\build_env.bat"
if errorlevel 1 exit /b 1

echo Cleaning build directory...
del /F /Q "%BUILD_DIR%\*.o" 2>nul
del /F /Q "%BUILD_DIR%\*.exe" 2>nul
del /F /Q "%BUILD_DIR%\*.exe.dbg" 2>nul

echo Restoring runtime files...
copy /Y "%COSMO%\crt.o" "%BUILD_DIR%\" > nul
copy /Y "%COSMO%\ape.o" "%BUILD_DIR%\" > nul
copy /Y "%COSMO%\ape.lds" "%BUILD_DIR%\" > nul
copy /Y "%COSMO%\cosmopolitan.a" "%BUILD_DIR%\" > nul

exit /b 0 