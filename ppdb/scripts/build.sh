#!/bin/bash

# Set proxy if provided
PROXY=""
if [ ! -z "$HTTP_PROXY" ]; then
    PROXY="$HTTP_PROXY"
elif [ ! -z "$HTTPS_PROXY" ]; then
    PROXY="$HTTPS_PROXY"
fi

if [ ! -z "$PROXY" ]; then
    echo "Using proxy: $PROXY"
fi

# Set paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."
ROOT_DIR="$(pwd)"
BUILD_DIR="$ROOT_DIR/build"
INCLUDE_DIR="$ROOT_DIR/include"
TEST_DIR="$ROOT_DIR/test"

# Set tool paths based on OS
COSMO="$ROOT_DIR/../repos/cosmopolitan"
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS toolchain (try to find LLVM tools in standard locations)
    for prefix in /usr/local/opt/llvm/bin /opt/homebrew/opt/llvm/bin; do
        if [ -d "$prefix" ]; then
            export PATH="$prefix:$PATH"
            break
        fi
    done
    CC="clang"
    AR="llvm-ar"
    OBJCOPY="llvm-objcopy"
elif [[ "$OSTYPE" == "linux"* ]]; then
    # Linux toolchain
    CC="gcc"
    AR="ar"
    OBJCOPY="objcopy"
else
    echo "Error: Unsupported operating system: $OSTYPE"
    exit 1
fi

# Verify tool paths
for cmd in $CC $AR $OBJCOPY; do
    if ! command -v $cmd &> /dev/null; then
        echo "Error: Required command '$cmd' not found"
        exit 1
    fi
done

# Get test type and build mode
TEST_TYPE="${1:-help}"
BUILD_MODE="${2:-debug}"

# Show help if requested
if [ "$TEST_TYPE" = "help" ]; then
    echo "PPDB Build and Test Tool"
    echo
    echo "Usage: build.sh [module] [build_mode]"
    echo
    echo "Available modules:"
    echo "  test42    Run basic test"
    echo "  sync      Run synchronization primitives test"
    echo "  skiplist  Run skiplist test"
    echo "  memtable  Run memtable test"
    echo "  sharded   Run sharded memtable test"
    echo "  kvstore   Run KVStore test"
    echo "  wal_core  Run WAL core test"
    echo "  wal_func  Run WAL functionality test"
    echo "  wal_advanced Run WAL advanced test"
    echo "  unit      Run unit tests"
    echo "  all       Run all tests"
    echo "  ppdb      Build main program"
    echo "  help      Show this help message"
    echo
    echo "Build modes:"
    echo "  debug     Debug mode (default)"
    echo "  release   Release mode"
    echo
    echo "Examples:"
    echo "  build.sh help              Show help message"
    echo "  build.sh ppdb              Build main program"
    echo "  build.sh test42            Run basic test"
    echo "  build.sh sync debug        Run sync test in debug mode"
    echo "  build.sh memtable release  Run memtable test in release mode"
    echo "  build.sh all               Run all tests"
    exit 0
fi

# Set compilation flags based on build mode
if [ "$BUILD_MODE" = "release" ]; then
    COMMON_FLAGS="-g -O2 -Wall -Wextra -fno-pie -fno-stack-protector -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -fno-asynchronous-unwind-tables"
    RELEASE_FLAGS="-DNDEBUG"
    BUILD_FLAGS="$COMMON_FLAGS $RELEASE_FLAGS"
else
    COMMON_FLAGS="-g -O0 -Wall -Wextra -fno-pie -fno-stack-protector -fno-omit-frame-pointer -mno-red-zone -fno-common -fno-plt -fno-asynchronous-unwind-tables"
    DEBUG_FLAGS="-DDEBUG"
    BUILD_FLAGS="$COMMON_FLAGS $DEBUG_FLAGS"
fi

# Set include paths
INCLUDE_FLAGS="-nostdinc -I$ROOT_DIR -I$ROOT_DIR/include -I$ROOT_DIR/src -I$ROOT_DIR/src/kvstore -I$ROOT_DIR/src/kvstore/internal -I$COSMO -I$TEST_DIR/white"

# Set final CFLAGS
CFLAGS="$BUILD_FLAGS $INCLUDE_FLAGS -include $COSMO/cosmopolitan.h"

# Set linker flags
LDFLAGS="-static -nostdlib -Wl,-T,$BUILD_DIR/ape.lds -Wl,--gc-sections -fuse-ld=bfd -Wl,-z,max-page-size=0x1000 -no-pie"
LIBS="$BUILD_DIR/crt.o $BUILD_DIR/ape.o $BUILD_DIR/cosmopolitan.a"

# Function to check if rebuild is needed
check_need_rebuild() {
    local src="$1"
    local obj="$2"
    if [ ! -f "$obj" ] || [ "$src" -nt "$obj" ]; then
        return 0
    fi
    return 1
}

# Set test name and extra sources based on test type
case "$TEST_TYPE" in
    "test42")
        TEST_NAME="test42"
        EXTRA_SOURCES=""
        ;;
    "sync")
        TEST_NAME="sync"
        EXTRA_SOURCES="src/kvstore/sync.c"
        ;;
    "skiplist")
        TEST_NAME="skiplist"
        EXTRA_SOURCES="src/kvstore/sync.c src/kvstore/internal/skiplist.c"
        ;;
    "memtable")
        TEST_NAME="memtable"
        EXTRA_SOURCES="src/kvstore/sync.c src/kvstore/internal/skiplist.c src/kvstore/internal/memtable.c"
        ;;
    "sharded")
        TEST_NAME="sharded"
        EXTRA_SOURCES="src/kvstore/sync.c src/kvstore/internal/skiplist.c src/kvstore/internal/memtable.c src/kvstore/internal/sharded_memtable.c"
        ;;
    "wal_core")
        TEST_NAME="wal_core"
        EXTRA_SOURCES="src/kvstore/sync.c src/kvstore/internal/wal.c"
        ;;
    "wal_func")
        TEST_NAME="wal_func"
        EXTRA_SOURCES="src/kvstore/sync.c src/kvstore/internal/wal.c"
        ;;
    "wal_advanced")
        TEST_NAME="wal_advanced"
        EXTRA_SOURCES="src/kvstore/sync.c src/kvstore/internal/wal.c"
        ;;
    "kvstore")
        TEST_NAME="kvstore"
        EXTRA_SOURCES="src/kvstore/sync.c src/kvstore/internal/skiplist.c src/kvstore/internal/memtable.c src/kvstore/internal/sharded_memtable.c src/kvstore/internal/wal.c src/kvstore/kvstore.c"
        ;;
    *)
        echo "Error: Unknown test type: $TEST_TYPE"
        exit 1
        ;;
esac

# First compile extra sources
if [ ! -z "$EXTRA_SOURCES" ]; then
    echo
    echo "===== Compiling extra sources ====="
    echo
    for src in $EXTRA_SOURCES; do
        obj="$BUILD_DIR/$(basename ${src%.*}).o"
        if check_need_rebuild "$ROOT_DIR/$src" "$obj"; then
            echo "Compiling $src..."
            "$CC" $CFLAGS -c "$ROOT_DIR/$src" -o "$obj"
            if [ $? -ne 0 ]; then
                echo "Error: Failed to compile $src"
                exit 1
            fi
        else
            echo "Object file $obj is up to date"
        fi
    done
fi

# Then compile test file to object file
echo
echo "===== Compiling test file ====="
echo
TEST_OBJ="$BUILD_DIR/test_${TEST_NAME}.o"
if [ ! -z "$TEST_FILE" ]; then
    TEST_SRC="$TEST_FILE"
else
    TEST_SRC="test/white/test_${TEST_NAME}.c"
fi

if check_need_rebuild "$ROOT_DIR/$TEST_SRC" "$TEST_OBJ"; then
    echo "Compiling $TEST_SRC..."
    "$CC" $CFLAGS -c "$ROOT_DIR/$TEST_SRC" -o "$TEST_OBJ"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to compile test file"
        exit 1
    fi
fi

# Finally link everything together
echo
echo "===== Linking ====="
echo

# Set output extension based on OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    EXT="osx"
elif [[ "$OSTYPE" == "linux"* ]]; then
    EXT="lnx"
else
    EXT="exe"
fi

TEST_EXE="$BUILD_DIR/${TEST_NAME}_test.$EXT"
DBG_EXE="$BUILD_DIR/${TEST_NAME}_test.dbg"

OBJ_FILES=""
for src in $EXTRA_SOURCES; do
    OBJ_FILES="$OBJ_FILES $BUILD_DIR/$(basename ${src%.*}).o"
done
OBJ_FILES="$OBJ_FILES $TEST_OBJ"

echo "Linking $DBG_EXE..."
"$CC" $LDFLAGS -o "$DBG_EXE" $OBJ_FILES $LIBS
if [ $? -ne 0 ]; then
    echo "Error: Failed to link test executable"
    exit 1
fi

echo "Converting to APE format..."
echo "Command: $OBJCOPY -S -O binary $DBG_EXE $TEST_EXE"
"$OBJCOPY" -S -O binary "$DBG_EXE" "$TEST_EXE"
if [ $? -ne 0 ]; then
    echo "Error: objcopy failed"
    exit 1
fi

# Set executable permission
chmod +x "$TEST_EXE"

exit 0 