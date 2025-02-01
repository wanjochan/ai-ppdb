#!/bin/bash

echo "=============================="

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

echo "Building sqlite3..."
sh "$(dirname "$0")/build_sqlite3.sh"
if [ $? -ne 0 ]; then
    exit 1
fi

echo "Building infra..."
sh "$(dirname "$0")/build_infra.sh"
if [ $? -ne 0 ]; then
    exit 1
fi

# 构建依赖库
echo "Building poly library..."
sh "$(dirname "$0")/build_poly.sh"
if [ $? -ne 0 ]; then
    exit 1
fi

# Verify poly library exists
POLY_LIB="${BUILD_DIR}/poly/libpoly.a"
if [ ! -f "$POLY_LIB" ]; then
    echo "Error: Poly library not found at $POLY_LIB"
    exit 1
fi

echo "Building ppdb..."
# 准备源文件列表
SOURCES=()

# 先添加服务实现文件
if [ "${ENABLE_RINETD}" = "1" ]; then
    SOURCES+=("${SRC_DIR}/internal/peer/peer_rinetd.c")
fi

if [ "${ENABLE_MEMKV}" = "1" ]; then
    SOURCES+=("${SRC_DIR}/internal/peer/peer_memkv.c")
fi

if [ "${ENABLE_SQLITE3}" = "1" ]; then
    SOURCES+=("${SRC_DIR}/internal/peer/peer_sqlite3.c")
fi

# 然后添加其他源文件
SOURCES+=(
    "${SRC_DIR}/internal/peer/peer_service.c"
    "${SRC_DIR}/ppdb/ppdb.c"
)

# 编译源文件
echo "Building sources..."
OBJECTS=()  # Initialize OBJECTS array
for src in "${SOURCES[@]}"; do
    obj="${BUILD_DIR}/obj/$(basename "${src}" .c).o"
    OBJECTS+=("$obj")
done

compile_files "${SOURCES[@]}" "${BUILD_DIR}/obj" "ppdb"

# 链接
echo "Linking..."

# 检查所有目标文件是否存在
for obj in "${OBJECTS[@]}"; do
    if [ ! -f "$obj" ]; then
        echo "Error: Object file not found: $obj"
        exit 1
    fi
done

# 检查 libinfra.a 是否存在
INFRA_LIB="${BUILD_DIR}/infra/libinfra.a"
if [ ! -f "$INFRA_LIB" ]; then
    echo "Error: Infra library not found at $INFRA_LIB"
    exit 1
fi

# 重新排序目标文件，确保服务实现在前面
ORDERED_OBJECTS=()
for src in "${SOURCES[@]}"; do
    base=$(basename "${src}")
    if [[ "$base" == "peer_rinetd.c" || "$base" == "peer_memkv.c" || "$base" == "peer_sqlite3.c" ]]; then
        ORDERED_OBJECTS+=("${BUILD_DIR}/obj/$(basename "${src}" .c).o")
    fi
done
for src in "${SOURCES[@]}"; do
    base=$(basename "${src}")
    if [[ "$base" != "peer_rinetd.c" && "$base" != "peer_memkv.c" && "$base" != "peer_sqlite3.c" ]]; then
        ORDERED_OBJECTS+=("${BUILD_DIR}/obj/$(basename "${src}" .c).o")
    fi
done

"${CC}" ${LDFLAGS} "${ORDERED_OBJECTS[@]}" \
    -L"${BUILD_DIR}/poly" -lpoly \
    -L"${BUILD_DIR}/infra" -linfra \
    -L"${BUILD_DIR}/sqlite3" -lsqlite3 \
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
