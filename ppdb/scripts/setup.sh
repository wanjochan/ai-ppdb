#!/bin/bash

# Set paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."
ROOT_DIR="$(pwd)"

echo "=== PPDB Environment Setup Script ==="
echo

# Check required commands
for cmd in curl unzip git; do
    if ! command -v $cmd &> /dev/null; then
        echo "Error: Required command '$cmd' not found"
        exit 1
    fi
done

# Detect OS and check compiler toolchain
if [[ "$OSTYPE" == "darwin"* ]]; then
    # Check macOS toolchain
    for cmd in clang llvm-ar llvm-objcopy; do
        if ! command -v $cmd &> /dev/null; then
            echo "Error: Required command '$cmd' not found"
            echo "Please install llvm tools: brew install llvm"
            exit 1
        fi
    done
elif [[ "$OSTYPE" == "linux"* ]]; then
    # Check Linux toolchain
    for cmd in gcc ar objcopy; do
        if ! command -v $cmd &> /dev/null; then
            echo "Error: Required command '$cmd' not found"
            echo "Please install build-essential package"
            exit 1
        fi
    done
else
    echo "Error: Unsupported operating system: $OSTYPE"
    exit 1
fi

# Create necessary directories
mkdir -p repos/cosmopolitan
mkdir -p tools
mkdir -p build

# Download and install cosmocc (for runtime files)
echo
echo "Downloading toolchains..."

if [ ! -d "tools/cosmocc/bin" ]; then
    echo "Downloading cosmocc..."
    curl -L "https://cosmo.zip/pub/cosmocc/cosmocc.zip" -o cosmocc.zip
    echo "Extracting cosmocc..."
    unzip -q cosmocc.zip -d tools/cosmocc
    echo "Copying runtime files..."
    cp -f tools/cosmocc/lib/cosmo/cosmopolitan.* repos/cosmopolitan/
    cp -f tools/cosmocc/lib/cosmo/ape.* repos/cosmopolitan/
    cp -f tools/cosmocc/lib/cosmo/crt.* repos/cosmopolitan/
    rm -f cosmocc.zip
else
    echo "cosmocc already exists, skipping"
fi

# Clone reference code
echo
echo "Cloning reference code..."
cd repos

if [ ! -d "leveldb" ]; then
    echo "Cloning leveldb..."
    git clone --depth 1 --single-branch --no-tags https://github.com/google/leveldb.git
else
    echo "leveldb already exists, skipping"
fi

cd ..

# Copy runtime files to build directory
echo
echo "Preparing build directory..."
cp -f repos/cosmopolitan/ape.lds build/
cp -f repos/cosmopolitan/crt.o build/
cp -f repos/cosmopolitan/ape.o build/
cp -f repos/cosmopolitan/cosmopolitan.a build/

# Verify environment
echo
echo "Verifying environment..."

# Check runtime files
if [ ! -f "repos/cosmopolitan/cosmopolitan.h" ]; then
    echo "Error: cosmopolitan runtime files not properly installed"
    exit 1
fi

# Verify compiler
echo "int main() { return 0; }" > test.c
if [[ "$OSTYPE" == "darwin"* ]]; then
    clang test.c -o test.com
else
    gcc test.c -o test.com
fi
if [ $? -ne 0 ]; then
    echo "Error: compilation test failed"
    rm -f test.c
    exit 1
fi
rm -f test.c test.com

echo
echo "=== Environment setup complete ==="
echo "You can now start building PPDB"
echo "Run 'scripts/build.sh help' to see build options"

exit 0 