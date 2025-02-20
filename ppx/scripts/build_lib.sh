#!/bin/bash

# Error handling function
handle_error() {
    local exit_code=$1
    local error_message=$2
    if [ $exit_code -ne 0 ]; then
        echo "Error: ${error_message}"
        exit $exit_code
    fi
}

# Check if rebuild is needed
need_rebuild() {
    local target=$1
    shift
    local sources=("$@")
    
    # If target doesn't exist, rebuild needed
    if [ ! -f "$target" ]; then
        return 0
    fi
    
    # Check each source file
    for src in "${sources[@]}"; do
        if [ ! -f "$src" ]; then
            echo "Error: Source file not found: $src"
            exit 1
        fi
        if [ "$src" -nt "$target" ]; then
            return 0
        fi
    done
    
    return 1
}

# Build a static library
build_static_lib() {
    local build_dir=$1
    local lib_name=$2
    shift 2
    local sources=("$@")
    
    # Create build directory
    mkdir -p "${build_dir}"
    
    # Compile each source file
    local objects=()
    for src in "${sources[@]}"; do
        local obj="${build_dir}/$(basename "${src}" .c).o"
        objects+=("${obj}")
        
        # Check if rebuild is needed
        if [ ! -f "${obj}" ] || [ "${src}" -nt "${obj}" ]; then
            echo "Compiling: ${src}"
            "${CC}" ${CFLAGS} -c "${src}" -o "${obj}"
            handle_error $? "Failed to compile ${src}"
        else
            echo "Skipping: ${src} (up to date)"
        fi
    done
    
    # Create static library
    local lib="${build_dir}/lib${lib_name}.a"
    if need_rebuild "${lib}" "${objects[@]}"; then
        echo "Creating static library: ${lib}"
        "${AR}" rcs "${lib}" "${objects[@]}"
        handle_error $? "Failed to create static library"
    else
        echo "Static library is up to date: ${lib}"
    fi
}

# Build arch library
build_arch_lib() {
    local build_dir=$1
    shift
    local sources=("$@")
    
    # Create build directory
    mkdir -p "${build_dir}"
    
    # Compile each source file
    local objects=()
    for src in "${sources[@]}"; do
        local obj="${build_dir}/$(basename "${src}" .c).o"
        objects+=("${obj}")
        
        # Check if rebuild is needed
        if [ ! -f "${obj}" ] || [ "${src}" -nt "${obj}" ]; then
            echo "Compiling: ${src}"
            "${CC}" ${CFLAGS} -c "${src}" -o "${obj}"
            handle_error $? "Failed to compile ${src}"
        else
            echo "Skipping: ${src} (up to date)"
        fi
    done
    
    # Create static library
    local lib="${build_dir}/libarch.a"
    if need_rebuild "${lib}" "${objects[@]}"; then
        echo "Creating static library: ${lib}"
        "${AR}" rcs "${lib}" "${objects[@]}"
        handle_error $? "Failed to create static library"
    else
        echo "Static library is up to date: ${lib}"
    fi
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
        
        # 特殊处理 test_c1m （仅当 target_test 为空，即全量时跳过）
        if [ "${test_name}" = "test_c1m" ] && [ "${target_test}" = "" ]; then
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
