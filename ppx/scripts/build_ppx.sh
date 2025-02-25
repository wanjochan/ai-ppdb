#!/bin/bash

# Import common functions and environment variables
source "$(dirname "$0")/build_env.sh"

# Set compile flags with all necessary include paths
CFLAGS="-Os -fomit-frame-pointer -fno-pie -fno-pic -fno-common -fno-plt -mcmodel=large -finline-functions -I${PPX_DIR}/src -I${PPX_DIR}/include -I${SRC_DIR} -I${ROOT_DIR}/repos/cosmocc/include"

# Define source files
PPX_SOURCES=(
    "${SRC_DIR}/ppx.c"
    "${SRC_DIR}/internal/polyx/PolyxCmdline.c"
    "${SRC_DIR}/internal/polyx/PolyxConfig.c"
    "${SRC_DIR}/internal/polyx/PolyxService.c"
    "${SRC_DIR}/internal/polyx/PolyxServiceCmd.c"
    "${SRC_DIR}/internal/peerx/PeerxService.c"
    "${SRC_DIR}/internal/peerx/PeerxRinetd.c"
    "${SRC_DIR}/internal/peerx/PeerxSqlite.c"
    "${SRC_DIR}/internal/peerx/PeerxMemKV.c"
)

# Clean old build files
clean_build() {
    echo "Cleaning old build files..."
    rm -rf "${BUILD_DIR}/ppx"
    rm -f "${PPX_DIR}/ppx_latest.exe"
}

# Build PPX
build_ppx() {
    local build_dir="${BUILD_DIR}/ppx"
    local objects=()

    # Create build directory
    mkdir -p "${build_dir}"

    # Build arch library first
    echo "Building arch library..."
    sh "${PPX_DIR}/scripts/build_arch.sh"
    if [ $? -ne 0 ]; then
        echo "Failed to build arch library"
        exit 1
    fi

    # Compile all source files
    echo "Building PPX..."
    for src in "${PPX_SOURCES[@]}"; do
        local obj="${build_dir}/$(basename "${src}" .c).o"
        
        echo "Compiling: ${src}"
        "${CC}" ${CFLAGS} -c "${src}" -o "${obj}"
        if [ $? -ne 0 ]; then
            echo "Error: Failed to compile ${src}"
            exit 1
        fi
        objects+=("${obj}")
    done

    # Link everything together
    echo "Linking PPX..."
    "${CC}" ${CFLAGS} "${objects[@]}" -L"${BUILD_DIR}/arch" -larch -o "${build_dir}/ppx_latest.exe"
    if [ $? -ne 0 ]; then
        echo "Failed to link PPX"
        exit 1
    fi

    # Copy to target directory
    echo "Installing PPX..."
    cp -v "${build_dir}/ppx_latest.exe" "${PPX_DIR}/ppx_latest.exe"
    if [ $? -ne 0 ]; then
        echo "Failed to install PPX"
        exit 1
    fi

    ls -l "${PPX_DIR}/ppx_latest.exe"
}

# Main execution
clean_build
build_ppx 