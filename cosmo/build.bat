@echo off
setlocal

set MODULE=%1
if "%MODULE%"=="" (
  echo Please specify module name
  exit /b 1
)

rem Build test module
..\cross9\bin\x86_64-pc-linux-gnu-gcc.exe -c -mcmodel=small -fPIC -nostdlib -nostdinc -fno-stack-protector test.c -o build\test.o -I..\cosmopolitan -I..\cosmopolitan\libc\isystem
if %errorlevel% neq 0 exit /b %errorlevel%

..\cross9\bin\x86_64-pc-linux-gnu-gcc.exe -nostdlib -static -T dll.lds -Wl,--gc-sections -Wl,--build-id=none build\test.o -o build\test.dbg
if %errorlevel% neq 0 exit /b %errorlevel%

..\cross9\bin\x86_64-pc-linux-gnu-objcopy.exe -O binary build\test.dbg build\test.dat
if %errorlevel% neq 0 exit /b %errorlevel%

copy /Y build\test.dat test.dat
if %errorlevel% neq 0 exit /b %errorlevel%
