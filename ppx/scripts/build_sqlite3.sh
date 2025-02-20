#!/bin/bash

# Import common functions and environment variables
source "$(dirname "$0")/build_env.sh"
source "$(dirname "$0")/build_lib.sh"

# Set compile flags with all necessary include paths
CFLAGS="${CFLAGS} -I${PPX_DIR}/src -I${PPX_DIR}/include -I${SRC_DIR}"

# Create build directory
mkdir -p "${BUILD_DIR}/sqlite3"

# Build SQLite3
echo "Building SQLite3..."
SQLITE_LIB="${BUILD_DIR}/sqlite3/libsqlite3.a"
SQLITE_SRC="${PPX_DIR}/vendor/sqlite3/sqlite3.c"

# Check if rebuild is needed
if [ ! -f "${SQLITE_LIB}" ] || [ "${SQLITE_SRC}" -nt "${SQLITE_LIB}" ]; then
    # Compile
    echo "Compiling SQLite3..."
    "${CC}" ${CFLAGS} -c -o "${BUILD_DIR}/sqlite3/sqlite3.o" \
        "${SQLITE_SRC}"

    if [ $? -ne 0 ]; then
        echo "Error: Failed to compile sqlite3"
        exit 1
    fi

    # Create static library
    echo "Creating static library..."
    "${AR}" rcs "${SQLITE_LIB}" \
        "${BUILD_DIR}/sqlite3/sqlite3.o"

    if [ $? -ne 0 ]; then
        echo "Error: Failed to create sqlite3 static library"
        exit 1
    fi

    echo "SQLite3 build complete."
else
    echo "SQLite3 library is up to date, skipping build."
fi

# Copy to lib directory
mkdir -p "${PPX_DIR}/lib"
cp -vf "${SQLITE_LIB}" "${PPX_DIR}/lib/libsqlite3.a"

ls -al "${SQLITE_LIB}"