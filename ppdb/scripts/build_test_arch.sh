#!/bin/bash

# Import common functions and environment variables
source "$(dirname "$0")/build_env.sh"
source "$(dirname "$0")/build_common.sh"

# Create test directories if not exist
mkdir -p "${TEST_DIR}/arch"
mkdir -p "${SRC_DIR}/internal/arch"

# Build and run the test
build_and_run_test() {
    # First build the new architecture
    "${SCRIPT_DIR}/build_arch.sh"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to build architecture library"
        exit 1
    fi

    # Then build and run tests
    echo "Building and running tests..."
    cd "${TEST_DIR}/arch" || exit 1
    
    "${CC}" ${CFLAGS} \
        -I"${PPDB_DIR}/include" \
        -I"${PPDB_DIR}/src" \
        test_arch.c \
        -L"${BUILD_DIR}/arch" -larch \
        -o "${BUILD_DIR}/arch/test_arch"

    if [ $? -eq 0 ]; then
        echo "Running tests..."
        "${BUILD_DIR}/arch/test_arch"
    else
        echo "Error: Failed to build tests"
        exit 1
    fi
}

# Main execution
build_and_run_test 