@echo off
setlocal

set MODULE=%1
if "%MODULE%"=="" (
  echo Please specify module name
  exit /b 1
)

if not exist build mkdir build

rem Copy Cosmopolitan files if needed
if not exist build\cosmopolitan.h copy /y ..\cosmopolitan\cosmopolitan.h build\
if not exist build\cosmopolitan.a copy /y ..\cosmopolitan\cosmopolitan.a build\
if not exist build\ape.o copy /y ..\cosmopolitan\ape.o build\
if not exist build\crt.o copy /y ..\cosmopolitan\crt.o build\

rem Build test module
..\cross9\bin\x86_64-pc-linux-gnu-gcc.exe -c -g -Os -fno-pie -no-pie ^
    -mcmodel=large -mno-red-zone -nostdlib -nostdinc ^
    -fno-omit-frame-pointer -fno-stack-protector ^
    -fno-common -fno-plt -fno-asynchronous-unwind-tables ^
    -ffunction-sections -fdata-sections ^
    -masm=intel ^
    test.c -o build\test.o

if %errorlevel% neq 0 exit /b %errorlevel%

..\cross9\bin\x86_64-pc-linux-gnu-gcc.exe -g -Os -static -nostdlib ^
    -Wl,-T,dll.lds -Wl,--gc-sections -Wl,--build-id=none ^
    -Wl,-z,max-page-size=0x1000 -no-pie ^
    -fuse-ld=bfd ^
    build\test.o -o build\test.elf
if %errorlevel% neq 0 exit /b %errorlevel%

copy /Y build\test.elf test.elf
if %errorlevel% neq 0 exit /b %errorlevel%
