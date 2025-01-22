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
COSMO="${ROOT_DIR}/repos/cosmocc"
COSMOSRC="${ROOT_DIR}/repos/cosmopolitan"

# 设置工具链路径
CROSS9="${COSMO}/bin"
GCC="${CROSS9}/x86_64-pc-linux-gnu-gcc"
AR="${CROSS9}/x86_64-pc-linux-gnu-ar"
OBJCOPY="${CROSS9}/x86_64-pc-linux-gnu-objcopy"

# 验证工具链
for tool in "$GCC" "$AR" "$OBJCOPY"; do
    if [ ! -f "$tool" ]; then
        echo "Error: ${tool} not found"
        exit 1
    fi
done

# 设置构建标志
CFLAGS="-g -O2 -fno-pie -fno-pic -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -mcmodel=large"
LDFLAGS="-static -nostdlib -Wl,--gc-sections -Wl,--build-id=none -Wl,-z,max-page-size=4096"

# 验证运行时文件
for file in "crt.o" "ape.o" "cosmopolitan.a" "ape.lds"; do
    if [ ! -f "${COSMO}/$file" ]; then
        echo "Error: $file not found at ${COSMO}/$file"
        exit 1
    fi
done

# 导出环境变量
export ROOT_DIR PPDB_DIR BUILD_DIR CROSS9 GCC AR OBJCOPY COSMO COSMOSRC CFLAGS LDFLAGS TEST_DIR SRC_DIR 