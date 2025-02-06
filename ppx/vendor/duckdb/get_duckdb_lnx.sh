#!/bin/bash

# DuckDB version
VERSION="v1.1.3"

# Detect architecture
ARCH=$(uname -m)
case "$ARCH" in
    "x86_64")
        ARCH_NAME="linux_amd64"
        ;;
    "aarch64")
        ARCH_NAME="linux_aarch64"
        ;;
    *)
        echo "Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

# Create directories
mkdir -p duckdb
cd duckdb

# Download and extract DuckDB library
echo "Downloading DuckDB library for $ARCH_NAME..."
curl -L -o libduckdb.zip "https://github.com/duckdb/duckdb/releases/download/${VERSION}/libduckdb-${ARCH_NAME}.zip"
unzip -o libduckdb.zip
#rm libduckdb.zip

# Make library executable
chmod +x libduckdb.so

echo "DuckDB library downloaded successfully" 