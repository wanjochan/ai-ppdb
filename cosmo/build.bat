@echo off
if "%1"=="" (
    echo Usage: %0 ^<module^>
    exit /b 1
)

set MODULE=%1
set CROSS=..\cross9\bin\x86_64-pc-linux-gnu-
set CFLAGS=-Wall -Wextra -O2 -g -fPIC -fno-stack-protector -fno-asynchronous-unwind-tables -nostdinc
set LDFLAGS=-nostdlib -T dll.lds

if not exist build mkdir build

%CROSS%gcc.exe %CFLAGS% -c %MODULE%.c -o build\%MODULE%.o
if errorlevel 1 exit /b 1

%CROSS%ld.exe %LDFLAGS% -o build\%MODULE%.elf build\%MODULE%.o
if errorlevel 1 exit /b 1

copy /y build\%MODULE%.elf %MODULE%.elf > nul
