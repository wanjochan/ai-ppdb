@echo off
setlocal

set MODULE=%1
if "%MODULE%"=="" (
  echo Please specify module name
  exit /b 1
)

rem Build test module
..\cross9\bin\x86_64-pc-linux-gnu-gcc.exe -c -mcmodel=large -fPIC -nostdlib -nostdinc -fno-stack-protector test.c -o build\test.o -I..\cosmopolitan -I..\cosmopolitan\libc\isystem
if %errorlevel% neq 0 exit /b %errorlevel%

..\cross9\bin\x86_64-pc-linux-gnu-gcc.exe -nostdlib -static -T dll.lds -Wl,--gc-sections -Wl,--build-id=none -Wl,--no-relax -mcmodel=large build\test.o ..\cosmopolitan\cosmopolitan.a -o build\test.elf
if %errorlevel% neq 0 exit /b %errorlevel%

copy /Y build\test.elf test.elf
if %errorlevel% neq 0 exit /b %errorlevel%
