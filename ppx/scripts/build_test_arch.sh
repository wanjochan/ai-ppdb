#!/bin/bash

# Import common functions and environment variables
source "$(dirname "$0")/build_env.sh"

# Set compile flags with all necessary include paths
CFLAGS="-Os -fomit-frame-pointer -fno-pie -fno-pic -fno-common -fno-plt -mcmodel=large -finline-functions -I${PPX_DIR}/src -I${PPX_DIR}/include -I${SRC_DIR}"

# Define source files
ARCH_SOURCES=(
    "${SRC_DIR}/internal/infrax/InfraxCore.c"
    "${SRC_DIR}/internal/infrax/InfraxLog.c"
    "${SRC_DIR}/internal/arch/PpxInfra.c"
    "${SRC_DIR}/internal/infrax/InfraxMemory.c"
    "${SRC_DIR}/internal/infrax/InfraxThread.c"
    "${SRC_DIR}/internal/infrax/InfraxSync.c"
    "${SRC_DIR}/internal/infrax/InfraxNet.c"
)

# Define test sources
TEST_SOURCES=(
    "${TEST_DIR}/arch/test_arch.c"
    "${TEST_DIR}/arch/test_infrax_memory.c"
    "${TEST_DIR}/arch/test_infrax_error.c"
    "${TEST_DIR}/arch/test_infrax_thread.c"
    "${TEST_DIR}/arch/test_infrax_sync.c"
    "${TEST_DIR}/arch/test_infrax_net.c"
)

# Build the new architecture library
build_arch() {
    local build_dir="${BUILD_DIR}/arch"
    local lib_file="${build_dir}/libarch.a"
    local arch_objects=()
    local need_rebuild=0

    # Create build directory
    mkdir -p "${build_dir}"

    # Compile all source files
    echo "Building new architecture..."
    for src in "${ARCH_SOURCES[@]}"; do
        local obj="${build_dir}/$(basename "${src}" .c).o"
        
        echo "Compiling: ${src}"
        "${CC}" ${CFLAGS} -c "${src}" -o "${obj}"
        if [ $? -ne 0 ]; then
            echo "Error: Failed to compile ${src}"
            exit 1
        fi
        arch_objects+=("${obj}")
    done

    # Create static library
    echo "Creating static library: ${lib_file}"
    rm -f "${lib_file}"
    cd "${build_dir}" || exit 1
    "${AR}" rcs "${lib_file}" *.o
    if [ $? -ne 0 ]; then
        echo "Failed to create arch library"
        exit 1
    fi
    ls -l "${lib_file}"
}

# Build and run tests
build_tests() {
    local build_dir="${BUILD_DIR}/arch"
    local test_dir="${build_dir}/tests"
    
    mkdir -p "${test_dir}"

    # Compile and link tests
    echo "Building and running tests..."
    for src in "${TEST_SOURCES[@]}"; do
        local test_name="$(basename "${src}" .c)"
        local test_bin="${test_dir}/${test_name}"
        
        echo "Building test: ${test_name}"
        "${CC}" ${CFLAGS} "${src}" -L"${build_dir}" -larch -o "${test_bin}"
        
        if [ -x "${test_bin}" ]; then
            echo "Running test: ${test_name}"
            "${test_bin}"
        else
            echo "Error: Failed to build test ${test_name}"
            exit 1
        fi
    done
}

# Main execution
build_arch
if [ $? -eq 0 ]; then
    build_tests
fi
