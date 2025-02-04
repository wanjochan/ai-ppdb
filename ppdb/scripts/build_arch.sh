#!/bin/bash

# Import common functions and environment variables
source "$(dirname "$0")/build_env.sh"
source "$(dirname "$0")/build_common.sh"

# Define source files for new architecture
ARCH_SOURCES=(
    "${SRC_DIR}/internal/infra/InfraCore.c"
    "${SRC_DIR}/internal/infra/PpdbInfra.c"
    "${SRC_DIR}/internal/arch/PpdbArch.c"
)

# Define test sources
TEST_SOURCES=(
    "${TEST_DIR}/arch/test_arch.c"
)

# Build the new architecture library
build_arch() {
    local build_dir="${BUILD_DIR}/arch"
    local lib_file="${build_dir}/libarch.a"
    local arch_objects=()

    # Create build directory
    mkdir -p "${build_dir}"

    # Clean old library
    rm -vf "${lib_file}"

    # Set compile flags
    CFLAGS="${CFLAGS} -I${PPDB_DIR}/src -I${PPDB_DIR}/include"

    # Compile all source files
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
    cd "${build_dir}" || exit 1
    "${AR}" rcs "${lib_file}" *.o
    if [ $? -ne 0 ]; then
        echo "Failed to create arch library"
        exit 1
    fi

    # Show library info
    ls -lh "${lib_file}"
}

# Build and run tests
build_tests() {
    local build_dir="${BUILD_DIR}/arch"
    local test_dir="${build_dir}/tests"
    
    mkdir -p "${test_dir}"

    # Compile and link tests
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
echo "Building new architecture..."
build_arch

if [ $? -eq 0 ] && [ -n "${TEST_SOURCES[*]}" ]; then
    echo "Running tests..."
    build_tests
fi 