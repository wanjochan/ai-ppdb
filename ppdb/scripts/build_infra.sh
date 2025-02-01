#!/bin/bash

# 导入公共函数和环境变量
source "$(dirname "$0")/build_env.sh"
source "$(dirname "$0")/build_common.sh"

# 定义 infra 源文件
INFRA_SOURCES=(
    "${INFRA_DIR}/infra_core.c"
    "${INFRA_DIR}/infra_memory.c"
    "${INFRA_DIR}/infra_error.c"
    "${INFRA_DIR}/infra_net.c"
    "${INFRA_DIR}/infra_platform.c"
    "${INFRA_DIR}/infra_sync.c"
    "${INFRA_DIR}/infra_gc.c"
)

# 编译 infra 库
build_infra() {
    local build_dir="${BUILD_DIR}/infra"
    local lib_file="${build_dir}/libinfra.a"
    local infra_objects=()

    # 创建构建目录
    mkdir -p "${build_dir}"

    # 清理旧的库文件
    rm -f "${lib_file}"

    # 设置编译标志
    CFLAGS="${CFLAGS} -I${PPDB_DIR}/src"

    # 编译源文件
    compile_files "${INFRA_SOURCES[@]}" "${build_dir}" "infra"
    infra_objects=("${OBJECTS[@]}")

    # 检查所有目标文件是否存在
    for obj in "${infra_objects[@]}"; do
        if [ ! -f "$obj" ]; then
            echo "Error: Object file not found: $obj"
            exit 1
        fi
    done

    # 创建静态库
    echo "Creating static library..."
    "${AR}" rcs "${lib_file}" "${infra_objects[@]}"
    if [ $? -ne 0 ]; then
        echo "Failed to create infra library"
        exit 1
    fi

    # 显示库文件信息
    ls -lh "${lib_file}"
}

# 执行构建
build_infra
