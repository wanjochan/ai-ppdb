@echo off
setlocal

rem 编译插件
echo Building test11 plugin...
"..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-gcc.exe" -c ^
    -fno-pic -fno-pie -nostdinc ^
    -ffunction-sections -fdata-sections ^
    -O0 -Wall -Werror ^
    -fno-omit-frame-pointer ^
    -fno-inline -fno-optimize-sibling-calls ^
    -fkeep-static-functions ^
    test11.c -o test11.o

rem 链接插件
echo Linking plugin...
"..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-ld.exe" ^
    -static -nostdlib ^
    --entry=0 ^
    --section-start=.header=0 ^
    -T test11.lds ^
    --strip-debug ^
    test11.o -o test11.com.dbg

rem 生成最终插件
echo Generating final plugin...
"..\..\..\repos\cross9\bin\x86_64-pc-linux-gnu-objcopy.exe" ^
    -O binary ^
    -R .comment -R .note -R .eh_frame -R .rela* -R .gnu* -R .debug* ^
    test11.com.dbg test11.dl

echo Done.

endlocal 