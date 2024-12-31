@echo off
setlocal

set MODULE=%1
if "%MODULE%"=="" (
  echo Please specify module name
  exit /b 1
)

set COSMO_DIR=..\cosmopolitan
set CROSS_DIR=..\cross9\bin
set CC=%CROSS_DIR%\x86_64-pc-linux-gnu-gcc.exe
set LD=%CROSS_DIR%\x86_64-pc-linux-gnu-ld.exe
set OBJCOPY=%CROSS_DIR%\x86_64-pc-linux-gnu-objcopy.exe

if not exist build mkdir build

%CC% -g -Os -fno-pie -no-pie -mcmodel=large -mno-red-zone -nostdlib -nostdinc -o build\%MODULE%.dbg %MODULE%.c -I%COSMO_DIR% -I%COSMO_DIR%\libc\isystem -Wl,--gc-sections -Wl,-T,dll.lds -Wl,--build-id=none -include %COSMO_DIR%\cosmopolitan.h %COSMO_DIR%\cosmopolitan.a
%OBJCOPY% -S -O binary build\%MODULE%.dbg %MODULE%.dat
