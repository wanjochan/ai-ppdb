#!/bin/bash
# timeout 15s ./ppx/scripts/build_test_arch.sh

# Import common functions and environment variables
source "$(dirname "$0")/build_env.sh"

# Clean old build files
clean_build() {
    echo "Cleaning old build files..."
    rm -rf "${BUILD_DIR}/arch"
    rm -f "${TEST_DIR}/arch/test_arch"
    rm -f "${TEST_DIR}/arch/test_infrax_memory"
    rm -f "${TEST_DIR}/arch/test_infrax_error"
    rm -f "${TEST_DIR}/arch/test_infrax_thread"
    rm -f "${TEST_DIR}/arch/test_infrax_sync"
    rm -f "${TEST_DIR}/arch/test_infrax_net"
    rm -f "${TEST_DIR}/arch/test_infrax_async"
    rm -f "${TEST_DIR}/arch/test_polyx_async"
}

# Set compile flags with all necessary include paths
CFLAGS="-Os -fomit-frame-pointer -fno-pie -fno-pic -fno-common -fno-plt -mcmodel=large -finline-functions -I${PPX_DIR}/src -I${PPX_DIR}/include -I${SRC_DIR}"

# Define source files
ARCH_SOURCES=(
    "${SRC_DIR}/internal/infrax/InfraxCore.c"
    "${SRC_DIR}/internal/infrax/InfraxLog.c"
    "${SRC_DIR}/internal/arch/PpxInfra.c"
    "${SRC_DIR}/internal/infrax/InfraxMemory.c"
    "${SRC_DIR}/internal/infrax/InfraxSync.c"
    "${SRC_DIR}/internal/infrax/InfraxNet.c"
    "${SRC_DIR}/internal/infrax/InfraxThread.c"
    "${SRC_DIR}/internal/infrax/InfraxAsync.c"
    "${SRC_DIR}/internal/polyx/PolyxAsync.c"
)

# Define test sources
TEST_SOURCES=(
    "${TEST_DIR}/arch/test_arch.c"
    "${TEST_DIR}/arch/test_infrax_memory.c"
    "${TEST_DIR}/arch/test_infrax_error.c"
    "${TEST_DIR}/arch/test_infrax_sync.c"
    "${TEST_DIR}/arch/test_infrax_net.c"
    "${TEST_DIR}/arch/test_infrax_thread.c"
    "${TEST_DIR}/arch/test_infrax_async.c"
    "${TEST_DIR}/arch/test_polyx_async.c"
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
    local tests=()
    for src in "${TEST_SOURCES[@]}"; do
        local test_name="$(basename "${src}" .c)"
        local test_bin="${test_dir}/${test_name}"
        
        echo "Building test: ${test_name}"
        "${CC}" ${CFLAGS} "${src}" -L"${build_dir}" -larch -o "${test_bin}"
        
        if [ -x "${test_bin}" ]; then
            tests+=("${test_name}")
        else
            echo "Error: Failed to build test ${test_name}"
            exit 1
        fi
    done

    # Run tests
    for test in "${tests[@]}"; do
        echo "Running test: ${test}"
        test_bin="${BUILD_DIR}/arch/tests/${test}"
        "${test_bin}"
        if [ $? -ne 0 ]; then
            echo "Test ${test} failed. Stopping all tests."
            exit 1
        fi
    done
}

# Main execution
clean_build
build_arch
if [ $? -eq 0 ]; then
    build_tests
fi
