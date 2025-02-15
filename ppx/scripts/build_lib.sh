#!/bin/bash

# 构建 arch 库的核心函数
build_arch_lib() {
    local build_dir="$1"
    local sources=("${@:2}")  # 从第二个参数开始的所有参数作为源文件数组
    local lib_file="${build_dir}/libarch.a"
    local arch_objects=()
    local need_rebuild=0

    # Create build directory
    mkdir -p "${build_dir}"

    # Compile all source files
    echo "Building architecture library..."
    for src in "${sources[@]}"; do
        local obj="${build_dir}/$(basename "${src}" .c).o"
        
        # Check if we need to rebuild
        if [ ! -f "${obj}" ] || [ "${src}" -nt "${obj}" ]; then
            echo "Compiling: ${src}"
            "${CC}" ${CFLAGS} -c "${src}" -o "${obj}"
            if [ $? -ne 0 ]; then
                echo "Error: Failed to compile ${src}"
                return 1
            fi
            need_rebuild=1
        else
            echo "Skipping: ${src} (up to date)"
        fi
        arch_objects+=("${obj}")
    done

    # Create static library only if needed
    if [ ${need_rebuild} -eq 1 ] || [ ! -f "${lib_file}" ]; then
        echo "Creating static library: ${lib_file}"
        rm -f "${lib_file}"
        cd "${build_dir}" || return 1
        "${AR}" rcs "${lib_file}" *.o
        if [ $? -ne 0 ]; then
            echo "Failed to create arch library"
            return 1
        fi
        ls -l "${lib_file}"
    else
        echo "Static library is up to date: ${lib_file}"
    fi
    return 0
}

# 构建并运行测试的核心函数
build_and_run_tests() {
    local build_dir="$1"
    local target_test="$2"
    shift 2
    local test_sources=("$@")
    local test_dir="${build_dir}/tests"
    
    mkdir -p "${test_dir}"

    for src in "${test_sources[@]}"; do
        local test_name="$(basename "${src}" .c)"
        local test_bin="${test_dir}/${test_name}.exe"
        
        # Skip if target test is specified and doesn't match
        if [ -n "${target_test}" ] && [ "${test_name}" != "${target_test}" ]; then
            continue
        fi
        
        echo "Building test: ${test_name}"
        "${CC}" ${CFLAGS} "${src}" -L"${build_dir}" -larch -o "${test_bin}"
        if [ $? -ne 0 ]; then
            echo "Failed to build test: ${test_name}"
            return 1
        fi
        
        # 特殊处理 test_c1m
        if [ "${test_name}" = "test_c1m" ]; then
            echo "Skipping auto-run for ${test_name} (manual run required)"
            echo "bin=${test_bin}"
            continue
        fi

        echo "Running test: ${test_name}"
        "${test_bin}"
        if [ $? -ne 0 ]; then
            echo "Test failed: ${test_name}"
            return 1
        fi
    done
    return 0
} 