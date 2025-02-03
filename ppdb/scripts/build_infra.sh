#!/bin/bash

# 导入公共函数和环境变量
source "$(dirname "$0")/build_env.sh"
source "$(dirname "$0")/build_common.sh"

# 定义 infra 源文件
INFRA_SOURCES=(
    "${INFRA_DIR}/infra_platform.c"
    "${INFRA_DIR}/infra_memory.c"
    "${INFRA_DIR}/infra_sync.c"
    "${INFRA_DIR}/infra_thread.c"
    "${INFRA_DIR}/infra_error.c"
    "${INFRA_DIR}/infra_net.c"
    "${INFRA_DIR}/infra_gc.c"
    "${INFRA_DIR}/infra_core.c"
)

# 定义 infra 测试文件
INFRA_TEST_SOURCES=(
    "${TEST_DIR}/white/infra/test_infra_config.c"
)

# 编译 infra 库
build_infra() {
    local build_dir="${BUILD_DIR}/infra"
    local lib_file="${build_dir}/libinfra.a"
    local infra_objects=()

    # 创建构建目录
    mkdir -p "${build_dir}"

    # 清理旧的库文件
    echo rm "${lib_file}"
    rm -vf "${lib_file}"

    # 设置编译标志
    CFLAGS="${CFLAGS} -I${PPDB_DIR}/src"

    # 先编译所有源文件
    for src in "${INFRA_SOURCES[@]}"; do
        local obj="${build_dir}/$(basename "${src}" .c).o"
        "${CC}" ${CFLAGS} -I"${PPDB_DIR}/include" -I"${SRC_DIR}" -c "${src}" -o "${obj}"
        if [ $? -ne 0 ]; then
            echo "Error: Failed to compile ${src}"
            exit 1
        fi
        infra_objects+=("${obj}")
    done

    # 检查所有目标文件是否存在
    for obj in "${infra_objects[@]}"; do
        if [ ! -f "$obj" ]; then
            echo "Error: Object file not found: $obj"
            exit 1
        fi
        echo "Found object file: $obj"
    done

    # 创建静态库
    echo "Creating static library: ${lib_file}"
    cd "${build_dir}" || exit 1
    "${AR}" rcs "${lib_file}" *.o
    if [ $? -ne 0 ]; then
        echo "Failed to create infra library"
        exit 1
    fi

    # 验证库文件是否创建成功
    if [ ! -f "${lib_file}" ]; then
        echo "Error: Library file was not created: ${lib_file}"
        exit 1
    fi

    # 显示库文件信息
    echo ls "${lib_file}"
    ls -lh "${lib_file}"
}

# 编译和运行测试
build_and_run_tests() {
    local build_dir="${BUILD_DIR}/infra/tests"
    mkdir -p "${build_dir}"

    # 编译测试文件
    for test_src in "${INFRA_TEST_SOURCES[@]}"; do
        local test_name=$(basename "${test_src}" .c)
        local test_bin="${build_dir}/${test_name}"
        
        echo "Compiling test: ${test_src}"
        "${CC}" ${CFLAGS} -I"${PPDB_DIR}/src" -I"${PPDB_DIR}/test" \
            "${test_src}" "${BUILD_DIR}/infra/libinfra.a" -o "${test_bin}"
        
        if [ $? -ne 0 ]; then
            echo "Error: Failed to compile test ${test_src}"
            exit 1
        fi
        
        # 运行测试
        echo "Running test: ${test_name}"
        "${test_bin}"
        if [ $? -ne 0 ]; then
            echo "Error: Test ${test_name} failed"
            exit 1
        fi
    done
}

# 执行构建和测试
build_infra
build_and_run_tests
