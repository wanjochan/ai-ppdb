#!/bin/bash

# 加载环境变量和通用函数
source "$(dirname "$0")/build_env.sh"
if [ $? -ne 0 ]; then
    echo "Error: Failed to load build environment"
    exit 1
fi

# 创建构建目录（如果不存在）
mkdir -p "${BUILD_DIR}/poly"

# 编译所有 poly 源文件
echo -e "Building poly library..."

# 清理旧的库文件和目标文件
rm -f "${BUILD_DIR}/poly/libpoly.a"
rm -f "${BUILD_DIR}/poly"/*.o
rm -rf "${BUILD_DIR}/poly/.aarch64"

# 定义源文件
SRC_FILES=(
    "${SRC_DIR}/internal/poly/poly_memkv.c"
    "${SRC_DIR}/internal/poly/poly_memkv_cmd.c"
    "${SRC_DIR}/internal/poly/poly_db.c"
    "${SRC_DIR}/internal/poly/poly_plugin.c"
    "${SRC_DIR}/internal/poly/poly_poll.c"
)

# 设置最大并发数（根据CPU核心数）
MAX_JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)

# 编译函数
compile_file() {
    local src=$1
    local obj="${BUILD_DIR}/poly/$(basename "${src}" .c).o"
    
    # 检查源文件是否存在
    if [ ! -f "$src" ]; then
        echo -e "Error: Source file ${src} not found"
        return 1
    fi

    # 编译源文件
    echo "-e Compiling ${src}..."
    "${CC}" ${CFLAGS} -I"${PPDB_DIR}/include" -I"${SRC_DIR}" -I"${PPDB_DIR}/vendor/sqlite3" -I"${PPDB_DIR}/vendor/duckdb" -c "${src}" -o "${obj}"
    if [ $? -ne 0 ]; then
        echo "-e Error: Failed to compile ${src}"
        return 1
    fi
}

# 并行编译所有源文件
echo "Starting parallel compilation (max ${MAX_JOBS} jobs)..."
OBJECTS=()

for src in "${SRC_FILES[@]}"; do
    obj="${BUILD_DIR}/poly/$(basename "${src}" .c).o"
    OBJECTS+=("${obj}")
    if [ ! -f "${obj}" ] || [ "${src}" -nt "${obj}" ]; then
        compile_file "${src}"
        if [ $? -ne 0 ]; then
            echo "-e Error: Failed to compile ${src}"
            exit 1
        fi
    else
        echo "-e Skipping ${src} (up to date)"
    fi
done

# 创建静态库
echo "-e Creating static library..."
"${AR}" rcs "${BUILD_DIR}/poly/libpoly.a" "${OBJECTS[@]}"
if [ $? -ne 0 ]; then
    echo "-e Error: Failed to create static library"
    exit 1
fi

echo "-e Build complete."
ls -l "${BUILD_DIR}/poly/libpoly.a"
echo "-e Build completed in $SECONDS seconds."
