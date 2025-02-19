#!/bin/bash
# Usage: build_arch.sh [--clean]

# Import common functions and environment variables
source "$(dirname "$0")/build_env.sh"
source "$(dirname "$0")/build_lib.sh"

# Set compile flags with all necessary include paths
CFLAGS="${CFLAGS} -I${PPX_DIR}/src -I${PPX_DIR}/include -I${SRC_DIR}"

# Define source files for production build
ARCH_SOURCES=(
    "${SRC_DIR}/internal/infrax/InfraxCore.c"
    "${SRC_DIR}/internal/infrax/InfraxLog.c"
    "${SRC_DIR}/internal/arch/PpxInfra.c"
    "${SRC_DIR}/internal/infrax/InfraxMemory.c"
    "${SRC_DIR}/internal/infrax/InfraxThread.c"
    "${SRC_DIR}/internal/infrax/InfraxSync.c"
    "${SRC_DIR}/internal/infrax/InfraxAsync.c"
)

# Parse command line arguments
do_clean=0

for arg in "$@"; do
    case $arg in
        --clean)
            do_clean=1
            ;;
    esac
done

# Clean if requested
if [ $do_clean -eq 1 ]; then
    echo "Cleaning build directory..."
    rm -rf "${BUILD_DIR}/arch"
fi

# Build arch library
build_arch_lib "${BUILD_DIR}/arch" "${ARCH_SOURCES[@]}" 

cp -vf "${BUILD_DIR}/arch/libarch.a" "${PPX_DIR}/lib/libarch.a"
