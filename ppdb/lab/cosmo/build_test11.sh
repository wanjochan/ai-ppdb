#!/bin/bash

# 设置编译器路径
COSMOCC=cosmocc

# 编译插件
$COSMOCC -g -Os -static -nostdlib -fno-pie -no-pie \
    -ffunction-sections -fdata-sections \
    -Wl,--gc-sections \
    -o test11.com.dbg test11.c -T test11.lds

# 生成最终的动态库文件
objcopy -S -O binary test11.com.dbg test11.dl

# 编译主程序
$COSMOCC -g -Os -static -nostdlib -fno-pie -no-pie \
    -ffunction-sections -fdata-sections \
    -Wl,--gc-sections \
    -o test11_main.com.dbg test11_main.c -T test11_main.lds

# 生成最终的可执行文件
objcopy -S -O binary test11_main.com.dbg test11_main.com

# 导出符号
$COSMOCC -c test11.c -o test11.o
nm test11.o | grep -E "^[0-9a-f]+ [A-Z] " | cut -d' ' -f3 > test11.sym

echo "Build completed: test11.dl and test11_main.com" 