#!/bin/bash

# DuckDB version
VERSION="v1.1.3"

# Download and extract DuckDB library
echo "Downloading DuckDB library..."
curl -L -o libduckdb.zip --proxy http://127.0.0.1:8888 "https://github.com/duckdb/duckdb/releases/download/${VERSION}/libduckdb-osx-universal.zip"
unzip -o libduckdb.zip
#rm libduckdb.zip

# Make library executable
chmod +x libduckdb.dylib

echo "DuckDB library downloaded successfully" 
