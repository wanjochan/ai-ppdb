#!/bin/bash

# 设置编译器路径
COSMOCC=cosmocc

# 编译check_elf.c
$COSMOCC -o check_elf.com check_elf.c

# 编译gen_headers.c
$COSMOCC -o gen_headers.com gen_headers.c

# 编译主程序
$COSMOCC -g -Os -static -nostdlib -fno-pie -no-pie \
    -Wl,--gc-sections -o cosmo.com.dbg cosmo.c -T cosmo.lds

# 生成最终的可执行文件
objcopy -S -O binary cosmo.com.dbg cosmo.com

echo "Build completed: cosmo.com" 