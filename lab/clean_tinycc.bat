@echo off
chcp 65001 > nul
setlocal enabledelayedexpansion

REM Set directory
set TCC_DIR=d:\dev\ai-ppdb\cosmo_tinycc

REM Remove platform specific files
echo Removing platform specific files...
del /f /q "%TCC_DIR%\arm-*.c" 2>nul
del /f /q "%TCC_DIR%\arm-*.h" 2>nul
del /f /q "%TCC_DIR%\arm64-*.c" 2>nul
del /f /q "%TCC_DIR%\arm64-*.h" 2>nul
del /f /q "%TCC_DIR%\c67-*.c" 2>nul
del /f /q "%TCC_DIR%\c67-*.h" 2>nul
del /f /q "%TCC_DIR%\riscv64-*.c" 2>nul
del /f /q "%TCC_DIR%\riscv64-*.h" 2>nul
del /f /q "%TCC_DIR%\i386-*.c" 2>nul
del /f /q "%TCC_DIR%\i386-*.h" 2>nul

REM Remove unnecessary object format support
echo Removing unnecessary object format support...
del /f /q "%TCC_DIR%\tccpe.c" 2>nul
del /f /q "%TCC_DIR%\tccmacho.c" 2>nul
del /f /q "%TCC_DIR%\tcccoff.c" 2>nul

REM Remove unnecessary tools and docs
echo Removing unnecessary tools and docs...
rd /s /q "%TCC_DIR%\win32" 2>nul
rd /s /q "%TCC_DIR%\tests" 2>nul
rd /s /q "%TCC_DIR%\examples" 2>nul
del /f /q "%TCC_DIR%\tcc-doc.texi" 2>nul
del /f /q "%TCC_DIR%\texi2pod.pl" 2>nul

echo Cleanup complete!
