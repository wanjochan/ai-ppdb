#!/bin/bash

# Set proxy if provided
PROXY=""
if [ ! -z "$HTTP_PROXY" ]; then
    PROXY="$HTTP_PROXY"
elif [ ! -z "$HTTPS_PROXY" ]; then
    PROXY="$HTTPS_PROXY"
elif [ ! -z "$1" ]; then
    PROXY="$1"
fi

if [ ! -z "$PROXY" ]; then
    echo "Using proxy: $PROXY"
fi

# Set paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/../.."
ROOT_DIR="$(pwd)"
PPDB_DIR="$ROOT_DIR/ppdb"
BUILD_DIR="$PPDB_DIR/build"
INCLUDE_DIR="$PPDB_DIR/include"
TEST_DIR="$PPDB_DIR/test"

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
    echo "macOS detected. Setting up LLVM toolchain..."
    
    # Try to find LLVM tools in common locations
    LLVM_PATHS=(
        "/usr/local/opt/llvm/bin"
        "/opt/homebrew/opt/llvm/bin"
        "/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin"
    )
    
    LLVM_FOUND=0
    for prefix in "${LLVM_PATHS[@]}"; do
        if [ -d "$prefix" ]; then
            export PATH="$prefix:$PATH"
            LLVM_FOUND=1
            echo "Found LLVM tools at $prefix"
            break
        fi
    done
    
    if [ $LLVM_FOUND -eq 0 ]; then
        echo "LLVM tools not found. Installing via Homebrew..."
        if ! command -v brew &> /dev/null; then
            echo "Homebrew not found. Installing Homebrew..."
            /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        fi
        brew install llvm
        export PATH="/usr/local/opt/llvm/bin:$PATH"
    fi
    
    # Verify LLVM tools
    for cmd in clang llvm-ar llvm-objcopy; do
        if ! command -v $cmd &> /dev/null; then
            echo "Error: Required command '$cmd' not found after setup"
            exit 1
        fi
    done
elif [[ "$OSTYPE" == "linux"* ]]; then
    # Check Linux toolchain
    for cmd in gcc ar objcopy wget; do
        if ! command -v $cmd &> /dev/null; then
            echo "Error: Required command '$cmd' not found"
            echo "Please install build-essential and wget packages"
            exit 1
        fi
    done
else
    echo "Error: Unsupported operating system: $OSTYPE"
    exit 1
fi

# Create necessary directories
mkdir -p repos/cosmopolitan
mkdir -p ppdb/tools
mkdir -p ppdb/build

# Download and install cosmocc (for runtime files)
echo
echo "Downloading toolchains..."

if [ ! -d "repos/cosmocc/bin" ]; then
    echo "cosmocc not found or incomplete, starting download..."
    [ -d "repos/cosmocc" ] && rm -rf "repos/cosmocc"
    if [ ! -f "repos/cosmocc.zip" ]; then
        echo "Downloading cosmocc..."
        if [ ! -z "$PROXY" ]; then
            curl -x "$PROXY" -L --retry 10 --retry-delay 30 --max-time 300 --speed-limit 100 --speed-time 10 --retry-max-time 3600 --continue-at - --progress-bar "https://cosmo.zip/pub/cosmocc/cosmocc.zip" -o "repos/cosmocc.zip"
        else
            curl -L --retry 10 --retry-delay 30 --max-time 300 --speed-limit 100 --speed-time 10 --retry-max-time 3600 --continue-at - --progress-bar "https://cosmo.zip/pub/cosmocc/cosmocc.zip" -o "repos/cosmocc.zip"
        fi
    else
        echo "Using existing cosmocc.zip"
    fi
    
    echo "Extracting cosmocc..."
    if ! unzip -q "repos/cosmocc.zip" -d repos/cosmocc; then
        echo "Error: Failed to extract cosmocc"
        exit 1
    fi
    
    echo "Copying runtime files..."
    cp -f repos/cosmocc/lib/cosmo/cosmopolitan.* repos/cosmopolitan/
    cp -f repos/cosmocc/lib/cosmo/ape.* repos/cosmopolitan/
    cp -f repos/cosmocc/lib/cosmo/crt.* repos/cosmopolitan/
else
    echo "cosmocc exists and is complete, skipping"
fi

# Clone reference code
echo
echo "Cloning reference code..."
cd repos

if [ ! -d "leveldb" ]; then
    echo "Cloning leveldb..."
    if [ ! -z "$PROXY" ]; then
        if ! git -c http.proxy="$PROXY" clone --depth 1 --single-branch --no-tags https://github.com/google/leveldb.git; then
            echo "Error: Failed to clone leveldb"
            exit 1
        fi
    else
        if ! git clone --depth 1 --single-branch --no-tags https://github.com/google/leveldb.git; then
            echo "Error: Failed to clone leveldb"
            exit 1
        fi
    fi
else
    echo "leveldb already exists, skipping"
fi

cd ..

# Copy runtime files to build directory
echo
echo "Preparing build directory..."
cp -f repos/cosmopolitan/ape.lds ppdb/build/
cp -f repos/cosmopolitan/crt.o ppdb/build/
cp -f repos/cosmopolitan/ape.o ppdb/build/
cp -f repos/cosmopolitan/cosmopolitan.a ppdb/build/

# Verify environment
echo
echo "Verifying environment..."

# Check runtime files
if [ ! -f "repos/cosmopolitan/cosmopolitan.h" ]; then
    echo "Error: cosmopolitan runtime files not properly installed"
    exit 1
fi

# Run test42 as verification
echo "Running basic test..."
cd ppdb
if ! scripts/build.sh test42; then
    echo "Error: basic test failed"
    exit 1
fi

# Optional: APE Loader installation for Linux
if [[ "$OSTYPE" == "linux"* ]]; then
    echo "Do you want to install APE Loader? (y/n)"
    read answer
    if [ "$answer" = "y" ]; then
        if ! wget -O /usr/bin/ape https://cosmo.zip/pub/cosmos/bin/ape-$(uname -m).elf; then
            echo "Error: Failed to download APE Loader"
            exit 1
        fi
        sudo chmod +x /usr/bin/ape
        sudo sh -c "echo ':APE:M::MZqFpD::/usr/bin/ape:' >/proc/sys/fs/binfmt_misc/register"
        sudo sh -c "echo ':APE-jart:M::jartsr::/usr/bin/ape:' >/proc/sys/fs/binfmt_misc/register"
    fi
    
    # WSL configuration (using more robust detection)
    if grep -q "microsoft" /proc/version 2>/dev/null || grep -q "Microsoft" /proc/version 2>/dev/null; then
        echo "WSL detected. Do you want to configure WSL for APE? (y/n)"
        read answer
        if [ "$answer" = "y" ]; then
            sudo sh -c "echo -1 >/proc/sys/fs/binfmt_misc/WSLInterop"
        fi
    fi
fi

echo
echo "=== Environment setup complete ==="
echo "You can now start building PPDB"
echo "Run 'scripts/build.sh help' to see build options"

exit 0
