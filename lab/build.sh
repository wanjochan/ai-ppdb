#!/bin/bash

set -e  # Exit on error

# Check for binutils location
BINUTILS_PATH=""
if [ -d "/usr/local/opt/binutils/bin" ]; then
    BINUTILS_PATH="/usr/local/opt/binutils/bin"
elif [ -d "/opt/homebrew/opt/binutils/bin" ]; then
    BINUTILS_PATH="/opt/homebrew/opt/binutils/bin"
fi

echo "=== Building original dylib ==="
# Compile the shared library source as a standard dylib
cc -shared -fPIC -arch arm64 ape_dl_poc.c -o ape_dl_poc.dylib

# Sign the original dylib
codesign --force -s - ape_dl_poc.dylib

echo "=== Building test program ==="
# Compile the test program
cc -arch arm64 test_dl.c -o test_dl

echo "=== Testing original dylib ==="
# Test the original dylib first
DYLD_LIBRARY_PATH=. ./test_dl

echo "=== Building PE+ELF+dylib ==="
# Generate multi-format file
cc make_ape_dl.c -o make_ape_dl
./make_ape_dl ape_dl_poc.dylib libape_test.so

# Copy and sign for testing
cp libape_test.so libape_test.dylib
codesign --force -s - libape_test.dylib

echo "=== Testing PE+ELF+dylib ==="
DYLD_LIBRARY_PATH=. ./test_dl

# Examine the files
echo "=== File analysis ==="
echo "--- File command output ---"
file ape_dl_poc.dylib
file libape_test.so
file libape_test.dylib

echo "--- Hexdump analysis ---"
hexdump -C libape_test.so | head -n 20

echo "--- ELF analysis ---"
if [ -n "$BINUTILS_PATH" ]; then
    echo "ELF header information:"
    "$BINUTILS_PATH/readelf" -h libape_test.so || echo "Not a valid ELF file"
    echo "ELF program headers:"
    "$BINUTILS_PATH/readelf" -l libape_test.so || echo "No program headers found"
else
    echo "binutils not found in standard locations. Please check your installation."
    echo "Expected locations:"
    echo "  - /usr/local/opt/binutils/bin"
    echo "  - /opt/homebrew/opt/binutils/bin"
fi

# Clean up
rm -f ape_dl_poc.dylib make_ape_dl libape_test.dylib 