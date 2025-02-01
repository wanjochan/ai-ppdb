#!/bin/bash

# 加载通用构建脚本
source "$(dirname "$0")/build_common.sh" || { echo "Error: Failed to load build_common.sh"; exit 1; }

# 创建构建目录（如果不存在）
mkdir -p "${BUILD_DIR}/obj"

# 设置条件编译选项
ENABLE_RINETD=1
ENABLE_MEMKV=1

# 添加条件编译宏定义
if [ "${ENABLE_RINETD}" = "1" ]; then
    CFLAGS="${CFLAGS} -DDEV_RINETD"
fi

if [ "${ENABLE_MEMKV}" = "1" ]; then
    CFLAGS="${CFLAGS} -DDEV_MEMKV"
fi

# 清理旧的可执行文件
echo "remove ${BUILD_DIR}/ppdb_latest.exe"
rm -f "${BUILD_DIR}/ppdb_latest.exe"
rm -f "${PPDB_DIR}/ppdb_latest.exe"
echo "Building ppdb..."

# 构建依赖库
echo "Building poly library..."
"$(dirname "$0")/build_poly.sh"
if [ $? -ne 0 ]; then
    exit 1
fi

# Verify poly library exists
POLY_LIB="${BUILD_DIR}/poly/libpoly.a"
if [ ! -f "$POLY_LIB" ]; then
    echo "Error: Poly library not found at $POLY_LIB"
    exit 1
fi

echo "Building infra..."
"$(dirname "$0")/build_infra.sh"
if [ $? -ne 0 ]; then
    exit 1
fi

echo "Building sqlite3..."
"$(dirname "$0")/build_sqlite3.sh"
if [ $? -ne 0 ]; then
    exit 1
fi

# 准备源文件列表
SOURCES=(
    "${SRC_DIR}/internal/poly/poly_cmdline.c"
    "${SRC_DIR}/internal/poly/poly_atomic.c"
    "${SRC_DIR}/internal/peer/peer_service.c"
    "${SRC_DIR}/ppdb/ppdb.c"
)

# 根据条件添加源文件
if [ "${ENABLE_RINETD}" = "1" ]; then
    SOURCES+=("${SRC_DIR}/internal/peer/peer_rinetd.c")
fi

if [ "${ENABLE_MEMKV}" = "1" ]; then
    SOURCES+=("${SRC_DIR}/internal/peer/peer_memkv.c")
fi

# 编译源文件
echo "Building sources..."
compile_files "${SOURCES[@]}" "${BUILD_DIR}/obj" "ppdb"
# 链接
echo "Linking..."
#wait # 等待编译完成
sleep 2
"${CC}" ${LDFLAGS} "${OBJECTS[@]}" \
    -L"${BUILD_DIR}/poly" -lpoly \
    -L"${BUILD_DIR}/sqlite3" -lsqlite3 \
    -L"${BUILD_DIR}/infra" -linfra \
    -o "${BUILD_DIR}/ppdb_latest.exe"
if [ $? -ne 0 ]; then
    echo "Error: Linking failed"
    rm -f "${BUILD_DIR}/ppdb_latest.exe"
    exit 1
fi

# 复制可执行文件到目标目录
echo "copy to ppdb/"
cp -v "${BUILD_DIR}/ppdb_latest.exe" "${PPDB_DIR}/ppdb_latest.exe"
ls -al "${PPDB_DIR}/ppdb_latest.exe"

exit 0
