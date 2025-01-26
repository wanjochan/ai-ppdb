#!/bin/bash

set -e  # Exit on error

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

echo "=== Building PE+dylib ==="
# Generate PE+dylib file
cc make_ape_dl.c -o make_ape_dl
./make_ape_dl ape_dl_poc.dylib libape_test.so

# Copy and sign for testing
cp libape_test.so libape_test.dylib
codesign --force -s - libape_test.dylib

echo "=== Testing PE+dylib ==="
DYLD_LIBRARY_PATH=. ./test_dl

# Examine the files
echo "=== File analysis ==="
file ape_dl_poc.dylib
file libape_test.so
file libape_test.dylib
hexdump -C libape_test.so | head -n 20

# Clean up
rm -f ape_dl_poc.dylib make_ape_dl libape_test.dylib 