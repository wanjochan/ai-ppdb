#!/bin/bash

# 设置编译器路径
COSMOCC=cosmocc

# 编译模板
$COSMOCC -g -Os -static -nostdlib -fno-pie -no-pie \
    -ffunction-sections -fdata-sections \
    -Wl,--gc-sections \
    -o tpl_dl.com.dbg tpl_dl.c -T tpl_dl.lds

# 生成最终的动态库文件
objcopy -S -O binary tpl_dl.com.dbg tpl_dl.dl

# 导出符号
$COSMOCC -c tpl_dl.c -o tpl_dl.o
nm tpl_dl.o | grep -E "^[0-9a-f]+ [A-Z] " | cut -d' ' -f3 > dl.syms

echo "Build completed: tpl_dl.dl" 