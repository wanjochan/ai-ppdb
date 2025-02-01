#!/bin/bash

# 见 docs/ARCH.md

# 加载通用构建脚本
source "$(dirname "$0")/build_common.sh" || { echo "Error: Failed to load build_common.sh"; exit 1; }

# 创建构建目录（如果不存在）
mkdir -p "${BUILD_DIR}/poly"

# 编译所有 poly 源文件
echo -e "Building poly library..."

# 清理旧的库文件和目标文件
rm -f "${BUILD_DIR}/poly/libpoly.a"

# 定义源文件
SRC_FILES=(
    "${SRC_DIR}/internal/poly/poly_memkv.c"
    "${SRC_DIR}/internal/poly/poly_memkv_cmd.c"
    "${SRC_DIR}/internal/poly/poly_db.c"
    "${SRC_DIR}/internal/poly/poly_cmdline.c"
    "${SRC_DIR}/internal/poly/poly_atomic.c"
    "${SRC_DIR}/internal/poly/poly_plugin.c"
    "${SRC_DIR}/internal/poly/poly_poll.c"
)

# 预先构建目标文件列表
OBJECTS=()
for src in "${SRC_FILES[@]}"; do
    obj="${BUILD_DIR}/poly/$(basename "${src}" .c).o"
    OBJECTS+=("${obj}")
done

# 使用 build_common.sh 中的编译函数
compile_files "${SRC_FILES[@]}" "${BUILD_DIR}/poly" "poly"

# 确保所有目标文件都存在
for obj in "${OBJECTS[@]}"; do
    if [ ! -f "${obj}" ]; then
        echo "-e Error: Object file not found: ${obj}"
        exit 1
    fi
done

# 创建静态库
echo "-e Creating static library..."
"${AR}" rcs "${BUILD_DIR}/poly/libpoly.a" "${OBJECTS[@]}"
if [ $? -ne 0 ]; then
    echo "-e Error: Failed to create static library"
    exit 1
fi

# 确保静态库已创建
if [ ! -f "${BUILD_DIR}/poly/libpoly.a" ]; then
    echo "-e Error: Static library not created"
    exit 1
fi

echo "-e Build complete."
ls -l "${BUILD_DIR}/poly/libpoly.a"
echo "-e Build completed in $SECONDS seconds."
