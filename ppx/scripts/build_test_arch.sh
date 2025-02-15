#!/bin/bash
# timeout 60s ./ppx/scripts/build_test_arch.sh
# Usage: build_test_arch.sh [test_name] [--clean]

# Import common functions and environment variables
source "$(dirname "$0")/build_env.sh"
source "$(dirname "$0")/build_lib.sh"

# Set compile flags with all necessary include paths
CFLAGS="-Os -fomit-frame-pointer -fno-pie -fno-pic -fno-common -fno-plt -mcmodel=large -finline-functions -I${PPX_DIR}/src -I${PPX_DIR}/include -I${SRC_DIR} -I${ROOT_DIR}/repos/cosmocc/include"

# Define source files for test build
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
    "${TEST_DIR}/arch/test_c1m.c"
    "${TEST_DIR}/arch/test_cosmopolitan.c"
)

# Clean build and test files
clean_build() {
    echo "Cleaning old build files..."
    rm -rf "${BUILD_DIR}/arch"
    for test in "${TEST_SOURCES[@]}"; do
        rm -f "${TEST_DIR}/arch/$(basename "${test}" .c)"
    done
}

# Parse command line arguments
target_test=""
do_clean=0

for arg in "$@"; do
    case $arg in
        --clean)
            do_clean=1
            ;;
        *)
            if [ -z "$target_test" ]; then
                target_test="$arg"
            fi
            ;;
    esac
done

# Clean if requested
if [ $do_clean -eq 1 ]; then
    clean_build
fi

# Build arch library
if build_arch_lib "${BUILD_DIR}/arch" "${ARCH_SOURCES[@]}"; then
    # Build and run tests
    build_and_run_tests "${BUILD_DIR}/arch" "$target_test" "${TEST_SOURCES[@]}"
fi
