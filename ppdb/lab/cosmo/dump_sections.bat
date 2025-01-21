@echo off
if "%1"=="" (
    echo Usage: %0 ^<file^>
    exit /b 1
)

echo === Section Headers ===
..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-objdump.exe -h %1
echo.

echo === Section Details ===
..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-objdump.exe -s %1
echo.

echo === Section Relocations ===
..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-objdump.exe -r %1
echo. 