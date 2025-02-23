#!/bin/bash

# Set up build environment
BUILD_DIR="build/ppx/.aarch64"
SRC_DIR="src/internal/polyx"
INFRAX_DIR="src/internal/infrax"
INCLUDE_DIR="include"
SRC_INCLUDE_DIR="src"

# Create build directory if it doesn't exist
mkdir -p $BUILD_DIR

# Compile benchmark test
gcc -o $BUILD_DIR/test_polyxscript_benchmark \
    $SRC_DIR/test_polyxscript_benchmark.c \
    $SRC_DIR/PolyxScript.c \
    $INFRAX_DIR/InfraxCore.c \
    $INFRAX_DIR/InfraxMemory.c \
    -I$INCLUDE_DIR \
    -I$SRC_INCLUDE_DIR \
    -Wall -Wextra -O2

# Run benchmark test if compilation succeeds
if [ $? -eq 0 ]; then
    echo "Compilation successful. Running benchmarks..."
    $BUILD_DIR/test_polyxscript_benchmark
else
    echo "Compilation failed."
    exit 1
fi 