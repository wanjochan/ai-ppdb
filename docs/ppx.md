# PPX (Polyx Proxy) Project Documentation

## Overview
PPX is a proxy service framework that provides various proxy services including:
- Network forwarding (rinetd)
- Memory-based key-value store (memkv)
- SQLite-based storage service
- Disk-based key-value store (diskv)

## Project Structure

```
ppx/
├── bin/           # Binary output directory
├── build/         # Build artifacts
├── include/       # Public header files
├── lib/           # Library files
├── script/        # Build and utility scripts
├── scripts/       # Additional scripts
├── src/          # Source code
│   ├── internal/  # Internal implementation
│   │   ├── arch/     # Architecture specific code
│   │   ├── infrax/   # Infrastructure layer
│   │   ├── polyx/    # Core proxy functionality
│   │   └── Peerx/    # Peer services implementation
│   └── ppx.c     # Main entry point
├── test/         # Test files
│   ├── arch/     # Architecture tests
│   └── black/    # Black box tests
└── vendor/       # Third-party dependencies
```

## Core Components

### 1. InfraxCore
The foundation layer providing basic system services:
- Memory management
- Threading
- Logging
- Network operations
- Synchronization primitives

### 2. PolyxScript
A scripting engine that allows:
- Command execution
- Configuration parsing
- Runtime service management

### 3. PeerxServices
Implementation of various proxy services:
- Rinetd: Network port forwarding
- MemKV: Memory-based key-value store
- SQLite: SQL database service
- DiskV: Persistent key-value store

## Configuration

The system uses several configuration files:
- `ppdb.conf`: Main configuration for service endpoints
- `memkv.conf`: Memory KV store settings
- `sqlite3.conf`: SQLite database settings
- `rinetd.conf`: Port forwarding rules

Example ppdb.conf:
```
127.0.0.1  8018  127.0.0.1 8080 # rinetd forwarding
127.0.0.1 5433 sqlite file::memory:?cache=shared # SQLite service
127.0.0.1 11211 memkv sqlite::memory:?cache=shared # MemKV service
127.0.0.1 15432 diskv sqlite:./diskv.db # DiskV service
```

## Testing

The project includes two types of tests:
1. Architecture Tests (`test/arch/`)
   - Unit tests for individual components
   - Integration tests for component interactions

2. Black Box Tests (`test/black/`)
   - End-to-end functionality tests
   - Performance and stress tests

## Build System

The project uses a custom build system with scripts in the `script/` directory.
Key build artifacts are placed in:
- `bin/`: Executable files
- `lib/`: Library files
- `build/`: Intermediate build files

## Future Improvements

Areas identified for potential enhancement:
1. DuckDB Integration (planned)
2. Enhanced monitoring and metrics
3. Additional service types
4. Performance optimizations

## Development Guidelines

1. Code Organization:
   - Keep service implementations in appropriate directories
   - Follow existing naming conventions
   - Maintain clear separation of concerns

2. Testing:
   - Write unit tests for new features
   - Ensure black box tests cover end-to-end scenarios
   - Maintain test coverage

3. Documentation:
   - Update relevant configuration examples
   - Document new features and APIs
   - Keep this document current
