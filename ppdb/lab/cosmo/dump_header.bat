@echo off
if "%1"=="" (
    echo Usage: %0 ^<file^>
    exit /b 1
)

echo === File Info ===
dir %1
echo.

echo === Hex Dump ===
xxd -l 32 %1
echo.

echo === File Header ===
..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-objdump.exe -h %1
echo.

echo === Symbols ===
..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-nm.exe %1 