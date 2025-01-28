#!/bin/bash

# DuckDB version
VERSION="v1.1.3"

# Create directories
mkdir -p duckdb
cd duckdb

# Download and extract DuckDB library
echo "Downloading DuckDB library..."
curl -L -o libduckdb.zip "https://github.com/duckdb/duckdb/releases/download/${VERSION}/libduckdb-osx-universal.zip"
unzip -o libduckdb.zip
#rm libduckdb.zip

# Make library executable
chmod +x libduckdb.dylib

echo "DuckDB library downloaded successfully" 
