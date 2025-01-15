@echo off
setlocal EnableDelayedExpansion

REM 清理旧文件
del /q test9.o test9.dl test9.dl.dbg 2>nul

REM 编译
..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-gcc.exe -g -O2 -mcmodel=small -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -fPIC -Wall -Wextra -Wno-unused-parameter -nostdinc -I..\..\..\repos\cosmopolitan -I..\..\..\repos\cosmopolitan\libc -I..\..\..\repos\cosmopolitan\libc\calls -I..\..\..\repos\cosmopolitan\libc\sock -I..\..\..\repos\cosmopolitan\libc\thread -I.. -I..\.. -include ..\..\..\repos\cosmopolitan\cosmopolitan.h -c test9.c -o test9.o

REM 链接
..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-gcc.exe -nostdlib -shared -Wl,-T,dl.lds -Wl,-z,max-page-size=4096 -Wl,--build-id=none -Wl,-z,defs -Wl,--emit-relocs -Wl,--no-undefined -Wl,--gc-sections -o test9.dl test9.o 