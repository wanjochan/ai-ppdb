#!/bin/bash

# Import common functions and environment variables
source "$(dirname "$0")/build_env.sh"

# Set compile flags with all necessary include paths
CFLAGS="-Os -fomit-frame-pointer -fno-pie -fno-pic -fno-common -fno-plt -mcmodel=large -finline-functions -I${PPX_DIR}/src -I${PPX_DIR}/include -I${SRC_DIR} -I${ROOT_DIR}/repos/cosmocc/include"

# Define test source files
TEST_SOURCES=(
    "${TEST_DIR}/ppx/test_ppx.c"
    "${TEST_DIR}/ppx/test_polyx_cmdline.c"
    "${TEST_DIR}/ppx/test_polyx_config.c"
    "${TEST_DIR}/ppx/test_polyx_service.c"
    "${TEST_DIR}/ppx/test_peerx_service.c"
    "${TEST_DIR}/ppx/test_peerx_rinetd.c"
    "${TEST_DIR}/ppx/test_peerx_sqlite.c"
    "${TEST_DIR}/ppx/test_peerx_memkv.c"
)

# Clean old build files
clean_build() {
    echo "Cleaning old build files..."
    rm -rf "${BUILD_DIR}/ppx/tests"
    for src in "${TEST_SOURCES[@]}"; do
        local test_name="$(basename "${src}" .c)"
        rm -f "${TEST_DIR}/ppx/${test_name}"
    done
}

# Build and run tests
build_tests() {
    local build_dir="${BUILD_DIR}/ppx"
    local test_dir="${build_dir}/tests"
    local target_test="$1"
    
    mkdir -p "${test_dir}"

    # Build PPX first
    echo "Building PPX..."
    sh "${PPX_DIR}/scripts/build_ppx.sh"
    if [ $? -ne 0 ]; then
        echo "Failed to build PPX"
        exit 1
    fi

    # Compile and link tests
    echo "Building and running tests..."
    for src in "${TEST_SOURCES[@]}"; do
        local test_name="$(basename "${src}" .c)"
        local test_bin="${test_dir}/${test_name}.exe"
        
        # Skip if target test is specified and doesn't match
        if [ -n "${target_test}" ] && [ "${test_name}" != "${target_test}" ]; then
            continue
        fi
        
        echo "Building test: ${test_name}"
        "${CC}" ${CFLAGS} "${src}" -L"${build_dir}" -L"${BUILD_DIR}/arch" -larch -o "${test_bin}"
        if [ $? -ne 0 ]; then
            echo "Failed to build test: ${test_name}"
            exit 1
        fi
        ls -al "${test_bin}"
        
        echo "Running test: ${test_name}"
        "${test_bin}"
        if [ $? -ne 0 ]; then
            echo "Test failed: ${test_name}"
            exit 1
        fi
    done
}

# Main execution
clean_build
build_tests "$1" 