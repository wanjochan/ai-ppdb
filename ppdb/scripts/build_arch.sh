#!/bin/bash

# Import common functions and environment variables
source "$(dirname "$0")/build_env.sh"
source "$(dirname "$0")/build_common.sh"

# Set compile flags with all necessary include paths
CFLAGS="${CFLAGS} -I${PPDB_DIR}/src -I${PPDB_DIR}/include -I${SRC_DIR}"

# Define source files for new architecture
ARCH_SOURCES=(
    "${SRC_DIR}/internal/infra/InfraCore.c"
    "${SRC_DIR}/internal/infra/InfraLog.c"
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
    local need_rebuild=0

    # Create build directory
    mkdir -p "${build_dir}"

    # Compile all source files
    for src in "${ARCH_SOURCES[@]}"; do
        local obj="${build_dir}/$(basename "${src}" .c).o"
        
        # Check if we need to rebuild
        if [ ! -f "${obj}" ] || [ "${src}" -nt "${obj}" ]; then
            echo "Compiling: ${src}"
            "${CC}" ${CFLAGS} -c "${src}" -o "${obj}"
            if [ $? -ne 0 ]; then
                echo "Error: Failed to compile ${src}"
                exit 1
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
        cd "${build_dir}" || exit 1
        "${AR}" rcs "${lib_file}" *.o
        if [ $? -ne 0 ]; then
            echo "Failed to create arch library"
            exit 1
        fi
        ls -l "${lib_file}"
    else
        echo "Static library is up to date: ${lib_file}"
    fi
}

# Build and run tests
build_tests() {
    local build_dir="${BUILD_DIR}/arch"
    local test_dir="${build_dir}/tests"
    
    mkdir -p "${test_dir}"

    # Compile and link tests with all necessary include paths
    for src in "${TEST_SOURCES[@]}"; do
        local test_name="$(basename "${src}" .c)"
        local test_bin="${test_dir}/${test_name}"
        
        echo "Building test: ${test_name}"
        "${CC}" ${CFLAGS} -I"${PPDB_DIR}/include" -I"${PPDB_DIR}/src" -I"${SRC_DIR}" \
            "${src}" -L"${build_dir}" -larch -o "${test_bin}"
        
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