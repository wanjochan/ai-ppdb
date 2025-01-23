#!/bin/bash

# 设置目录路径
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
echo "ROOT_DIR=${ROOT_DIR}"

PPDB_DIR="${ROOT_DIR}/ppdb"
BUILD_DIR="${PPDB_DIR}/build"
BIN_DIR="${PPDB_DIR}/bin"
SRC_DIR="${PPDB_DIR}/src"
INCLUDE_DIR="${PPDB_DIR}/include"
INTERNAL_DIR="${PPDB_DIR}/internal"
TEST_DIR="${PPDB_DIR}/test"
# prebuilt software and mannually extract from curl -L -o cosmopolitan.zip https://justine.lol/cosmopolitan/cosmopolitan.zip
COSMOS="${ROOT_DIR}/repos/cosmos"
# cosmo compiler
COSMOCC="${ROOT_DIR}/repos/cosmocc"
# cosmo src
COSMOPOLITAN="${ROOT_DIR}/repos/cosmopolitan"

# 设置工具链路径
TOOLCHAIN_DIR="${COSMOCC}/bin"
CC="${TOOLCHAIN_DIR}/cosmocc"
AR="${TOOLCHAIN_DIR}/cosmoar"
OBJCOPY="${TOOLCHAIN_DIR}/objbincopy"

# 验证工具链
for tool in "$CC" "$AR" "$OBJCOPY"; do
    if [ ! -f "$tool" ]; then
        echo "Error: ${tool}"
        exit 1
    else
        echo "Found ${tool}"
    fi
done

# 设置构建标志
#CFLAGS="-g -O2 -fno-pie -fno-pic -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -mcmodel=large"
# cursor 推荐
CFLAGS="-Os -fomit-frame-pointer -fno-pie -fno-pic -fno-common -fno-plt -mcmodel=large"
LDFLAGS="-static -Wl,--gc-sections -Wl,--build-id=none"

# 使用 windsurf 推荐的 (failed)
# CFLAGS="-Os -fdata-sections -ffunction-sections -fno-unwind-tables -fno-asynchronous-unwind-tables"
# LDFLAGS="-static -Wl,--gc-sections -Wl,--strip-all -Wl,--build-id=none"

## 验证运行时文件
#for file in "crt.o" "ape.o" "ape.lds" "cosmopolitan.a" "cosmopolitan.h"; do
#    if [ ! -f "${COSMOS}/$file" ]; then
#        echo "Error: $file not found at ${COSMOS}/$file"
#        exit 1
#    else
#        echo "Found ${COSMOS}/$file"
#    fi
#done

# 导出环境变量
export ROOT_DIR PPDB_DIR BUILD_DIR CROSS9 CC AR OBJCOPY COSMOCC COSMOPOLITAN COSMOS CFLAGS LDFLAGS TEST_DIR SRC_DIR 
