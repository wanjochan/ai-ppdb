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
    
    # Build and run main arch test
    "${CC}" ${CFLAGS} \
        -I"${PPDB_DIR}/include" \
        -I"${PPDB_DIR}/src" \
        -I"${PPDB_DIR}" \
        test_arch.c \
        -L"${BUILD_DIR}/arch" -larch \
        -o "${BUILD_DIR}/arch/test_arch.exe"

    if [ $? -eq 0 ]; then
        echo "Running main arch tests..."
        "${BUILD_DIR}/arch/test_arch.exe"
    else
        echo "Error: Failed to build main arch tests"
        exit 1
    fi

    # Build and run infra log test
    "${CC}" ${CFLAGS} \
        -I"${PPDB_DIR}/include" \
        -I"${PPDB_DIR}/src" \
        -I"${PPDB_DIR}" \
        test_infrax_log.c \
        -L"${BUILD_DIR}/arch" -larch \
        -o "${BUILD_DIR}/arch/test_infrax_log.exe"

    if [ $? -eq 0 ]; then
        echo "Running infra log tests..."
        "${BUILD_DIR}/arch/test_infrax_log.exe"
    else
        echo "Error: Failed to build infra log tests"
        exit 1
    fi

    # Build and run memory management test
    "${CC}" ${CFLAGS} \
        -I"${PPDB_DIR}/include" \
        -I"${PPDB_DIR}/src" \
        -I"${PPDB_DIR}" \
        test_infrax_memory.c \
        -L"${BUILD_DIR}/arch" -larch \
        -o "${BUILD_DIR}/arch/test_infrax_memory.exe"

    if [ $? -eq 0 ]; then
        echo "Running memory management tests..."
        "${BUILD_DIR}/arch/test_infrax_memory.exe"
    else
        echo "Error: Failed to build memory management tests"
        exit 1
    fi

    # Build and run error handling test
    "${CC}" ${CFLAGS} \
        -I"${PPDB_DIR}/include" \
        -I"${PPDB_DIR}/src" \
        -I"${PPDB_DIR}" \
        -I"${PPDB_DIR}/test/white" \
        "${PPDB_DIR}/test/white/framework/test_framework.c" \
        test_infrax_error.c \
        -L"${BUILD_DIR}/arch" -larch \
        -o "${BUILD_DIR}/arch/test_infrax_error.exe"

    if [ $? -eq 0 ]; then
        echo "Running error handling tests..."
        "${BUILD_DIR}/arch/test_infrax_error.exe"
    else
        echo "Error: Failed to build error handling tests"
        exit 1
    fi
}

# Main execution
build_and_run_test