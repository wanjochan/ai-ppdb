@echo off
setlocal enabledelayedexpansion

:: DuckDB version
set VERSION=v1.1.3

:: Create directories
if not exist duckdb mkdir duckdb
cd duckdb

:: Detect architecture
if "%PROCESSOR_ARCHITECTURE%"=="AMD64" (
    set ARCH_NAME=windows_amd64
) else if "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
    set ARCH_NAME=windows_arm64
) else (
    echo Unsupported architecture: %PROCESSOR_ARCHITECTURE%
    exit /b 1
)

:: Download and extract DuckDB library
echo Downloading DuckDB library for %ARCH_NAME%...
curl -L -o libduckdb.zip "https://github.com/duckdb/duckdb/releases/download/%VERSION%/libduckdb-%ARCH_NAME%.zip"
powershell -command "Expand-Archive -Force libduckdb.zip ."
rem del libduckdb.zip

echo DuckDB library downloaded successfully 