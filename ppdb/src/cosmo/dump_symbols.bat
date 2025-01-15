@echo off
if "%1"=="" (
    echo Usage: %0 ^<file^>
    exit /b 1
)

echo === All Symbols ===
..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-nm.exe %1
echo.

echo === Dynamic Symbols ===
..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-objdump.exe -T %1
echo.

echo === Defined Functions ===
..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-nm.exe %1 | findstr " T " 