#!/bin/bash

# 加载环境变量和通用函数
source "$(dirname "$0")/build_env.sh"
if [ $? -ne 0 ]; then
    exit 1
fi

# 创建构建目录（如果不存在）
mkdir -p "${BUILD_DIR}/infra"

# 编译所有 infra 源文件
echo "Building infra library..."

# 清理旧的库文件
rm -f "${BUILD_DIR}/infra/libinfra.a"

# 编译所有源文件
INFRA_SOURCES=(
    "${SRC_DIR}/internal/infra/infra_core.c"
    "${SRC_DIR}/internal/infra/infra_memory.c"
    "${SRC_DIR}/internal/infra/infra_error.c"
    "${SRC_DIR}/internal/infra/infra_net.c"
    "${SRC_DIR}/internal/infra/infra_platform.c"
    "${SRC_DIR}/internal/infra/infra_sync.c"
)

# 编译每个源文件
for src in "${INFRA_SOURCES[@]}"; do
    obj="${BUILD_DIR}/infra/$(basename "${src}" .c).o"
    echo "Compiling ${src}..."
    set -x
    "${CC}" ${CFLAGS} -I"${SRC_DIR}" -I"${TOOLCHAIN_DIR}/include" -c "${src}" -o "${obj}"
    set +x
    if [ $? -ne 0 ]; then
        echo "Error: Failed to compile ${src}"
        exit 1
    fi
done

# 创建静态库
echo "Creating static library..."
set -x
"${AR}" rcs "${BUILD_DIR}/infra/libinfra.a" "${BUILD_DIR}"/infra/*.o
set +x
if [ $? -ne 0 ]; then
    echo "Error: Failed to create static library"
    exit 1
fi

echo "Build complete."
ls -l "${BUILD_DIR}/infra/libinfra.a"

exit 0 