#!/bin/bash

# 见 docs/ARCH.md

# 加载通用构建脚本
source "$(dirname "$0")/build_common.sh" || { echo "Error: Failed to load build_common.sh"; exit 1; }

# 创建构建目录（如果不存在）
mkdir -p "${BUILD_DIR}/poly"
mkdir -p "${BUILD_DIR}/poly/libdill"

# 首先编译 libdill 的核心文件
LIBDILL_SRC=(
    "${LIBDILL_DIR}/libdill.c"
    "${LIBDILL_DIR}/cr.c"
    "${LIBDILL_DIR}/handle.c"
    "${LIBDILL_DIR}/ctx.c"
    "${LIBDILL_DIR}/stack.c"
    "${LIBDILL_DIR}/pollset.c"
    "${LIBDILL_DIR}/utils.c"
    "${LIBDILL_DIR}/rbtree.c"
    "${LIBDILL_DIR}/fd.c"
    "${LIBDILL_DIR}/iol.c"
    "${SRC_DIR}/internal/poly/compat/now.c"
)

# 编译 libdill
echo "-e Building libdill..."
LIBDILL_OBJECTS=()
for src in "${LIBDILL_SRC[@]}"; do
    obj="${BUILD_DIR}/poly/libdill/$(basename "${src}" .c).o"
    LIBDILL_OBJECTS+=("${obj}")
    
    echo "-e Compiling ${src}..."
    "${CC}" -I"${LIBDILL_DIR}" -I"${SRC_DIR}/internal/poly/compat" -DDILL_THREADS -DDILL_SOCKETS -c "${src}" -o "${obj}"
    if [ $? -ne 0 ]; then
        echo "-e Error: Failed to compile ${src}"
        exit 1
    fi
done

# 创建 libdill 静态库
echo "-e Creating libdill static library..."
"${AR}" rcs "${BUILD_DIR}/poly/libdill.a" "${LIBDILL_OBJECTS[@]}"

# 定义源文件
SRC_FILES=(
    "${SRC_DIR}/internal/poly/poly_async.c"
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

# 使用 build_common.sh 中的编译函数，添加 libdill 链接
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
