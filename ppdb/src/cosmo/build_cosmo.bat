@echo off
setlocal

rem 编译主程序
"..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-gcc.exe" -g -O2 -mcmodel=small -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -Wall -Wextra -Wno-unused-parameter -nostdinc -I..\..\..\repos\cosmopolitan -I..\..\..\repos\cosmopolitan\libc -I..\..\..\repos\cosmopolitan\libc\calls -I..\..\..\repos\cosmopolitan\libc\sock -I..\..\..\repos\cosmopolitan\libc\thread -I.. -I..\.. -include ..\..\..\repos\cosmopolitan\cosmopolitan.h -c cosmo.c -o cosmo.o

rem 链接主程序
"..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-gcc.exe" -static -nostdlib -Wl,-T,..\..\..\repos\cosmopolitan\ape.lds -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,max-page-size=4096 -Wl,--defsym=ape_stack_vaddr=0x700000000000 -Wl,--defsym=ape_stack_memsz=0x100000 -Wl,--defsym=ape_stack_round=0x1000 -Wl,--entry=_start -o cosmo.exe.dbg cosmo.o ..\..\..\repos\cosmopolitan\crt.o ..\..\..\repos\cosmopolitan\ape.o ..\..\..\repos\cosmopolitan\cosmopolitan.a

rem 生成最终可执行文件
"..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-objcopy.exe" -S -O binary cosmo.exe.dbg cosmo.exe

endlocal 