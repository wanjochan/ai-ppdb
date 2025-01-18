@echo off
setlocal EnableDelayedExpansion

rem Build dynamic library
echo Building dynamic library...

rem Compile test8.c
echo Compiling test8.c...
set COMPILE_CMD="..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-gcc.exe" -g -O2 -mcmodel=small -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -Wall -Wextra -Wno-unused-parameter -nostdinc -I..\..\..\repos\cosmopolitan -I..\..\..\repos\cosmopolitan\libc -I..\..\..\repos\cosmopolitan\libc\calls -I..\..\..\repos\cosmopolitan\libc\sock -I..\..\..\repos\cosmopolitan\libc\thread -I.. -I..\.. -include ..\..\..\repos\cosmopolitan\cosmopolitan.h -c test8.c -o test8.o
echo Command: %COMPILE_CMD%
%COMPILE_CMD%
if %ERRORLEVEL% neq 0 (
    echo Failed to compile test8.c with error %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

rem Link test8.dl
echo Linking test8.dl...
set LINK_CMD="..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-gcc.exe" -nostdlib -shared -Wl,-T,test8.lds -Wl,-z,max-page-size=4096 -Wl,--build-id=none -o test8.dl test8.o
echo Command: %LINK_CMD%
%LINK_CMD%
if %ERRORLEVEL% neq 0 (
    echo Failed to link test8.dl with error %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

rem Build test program
echo Building test program...

rem Compile test8_main.c
echo Compiling test8_main.c...
set COMPILE_CMD="..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-gcc.exe" -g -O2 -mcmodel=small -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -Wall -Wextra -Wno-unused-parameter -nostdinc -I..\..\..\repos\cosmopolitan -I..\..\..\repos\cosmopolitan\libc -I..\..\..\repos\cosmopolitan\libc\calls -I..\..\..\repos\cosmopolitan\libc\sock -I..\..\..\repos\cosmopolitan\libc\thread -I.. -I..\.. -include ..\..\..\repos\cosmopolitan\cosmopolitan.h -c test8_main.c -o test8_main.o
echo Command: %COMPILE_CMD%
%COMPILE_CMD%
if %ERRORLEVEL% neq 0 (
    echo Failed to compile test8_main.c with error %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

rem Link test8_main.exe
echo Linking test8_main.exe...
set LINK_CMD="..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-gcc.exe" -static -nostdlib -Wl,-T,..\..\..\repos\cosmopolitan\ape.lds -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,max-page-size=4096 -Wl,--defsym=ape_stack_vaddr=0x700000000000 -Wl,--defsym=ape_stack_memsz=0x100000 -Wl,--defsym=ape_stack_round=0x1000 -Wl,--entry=_start -o test8_main.exe.dbg test8_main.o ..\..\..\repos\cosmopolitan\crt.o ..\..\..\repos\cosmopolitan\ape.o ..\..\..\repos\cosmopolitan\cosmopolitan.a
echo Command: %LINK_CMD%
%LINK_CMD%
if %ERRORLEVEL% neq 0 (
    echo Failed to link test8_main.exe with error %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

rem Create binary test8_main.exe
echo Creating binary test8_main.exe...
set OBJCOPY_CMD="..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-objcopy.exe" -S -O binary test8_main.exe.dbg test8_main.exe
echo Command: %OBJCOPY_CMD%
%OBJCOPY_CMD%
if %ERRORLEVEL% neq 0 (
    echo Failed to create binary test8_main.exe with error %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo Build complete. 