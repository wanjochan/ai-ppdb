# PPX (Polyx Proxy) Project Documentation

## Overview

PPX is a comprehensive proxy service framework that provides various network and storage services including:
- Network forwarding (rinetd) - TCP/UDP port forwarding
- Memory-based key-value store (memkv) - High-performance in-memory data storage
- SQLite-based storage service - SQL database access over the network
- Disk-based key-value store (diskv) - Persistent key-value storage

The framework is designed with a layered architecture focused on modularity, extensibility, and performance. It implements a C-language object-oriented design pattern using function pointers for method dispatch and clean interface separation.

## Architecture

PPX follows a layered architectural design with clear separation of concerns:

```
+------------------+
|  Peer Services   |  <- Service implementations (MemKV, Rinetd, SQLite)
+------------------+
|  Polyx System    |  <- Configuration, scripting, service management
+------------------+
|  Infrax Core     |  <- Infrastructure layer (memory, threads, network)
+------------------+
```

### Data Flow Overview

```
Client Request → Port Binding → Service Handler → Backend Processing → Response
```

1. Client connects to a configured port
2. PPX routes the request to the appropriate service handler
3. Service processes the request using its backend
4. Response is sent back to the client

## Project Structure

```
ppx/
├── bin/           # Binary output directory
├── build/         # Build artifacts
├── include/       # Public header files
│   ├── ppx/       # Public API headers
│   └── internal/  # Internal headers (not for external use)
├── lib/           # Library files
├── script/        # Build and utility scripts
├── scripts/       # Additional scripts and tools
├── src/           # Source code
│   ├── internal/  # Internal implementation
│   │   ├── arch/     # Architecture specific code
│   │   │   ├── PpxArch.[ch]     # Architecture entry point
│   │   │   └── PpxInfra.[ch]    # Infrastructure initialization
│   │   ├── infrax/   # Infrastructure layer
│   │   │   ├── InfraxCore.[ch]     # Core singleton implementation
│   │   │   ├── InfraxLog.[ch]      # Logging system
│   │   │   ├── InfraxMemory.[ch]   # Memory management
│   │   │   ├── InfraxThread.[ch]   # Thread management
│   │   │   ├── InfraxNet.[ch]      # Network operations
│   │   │   ├── InfraxSync.[ch]     # Synchronization primitives
│   │   │   └── InfraxAsync.[ch]    # Asynchronous operations
│   │   ├── polyx/    # Core proxy functionality
│   │   │   ├── PolyxScript.[ch]    # Scripting engine
│   │   │   ├── PolyxDB.[ch]        # Database interface
│   │   │   ├── PolyxService.[ch]   # Service management
│   │   │   ├── PolyxConfig.[ch]    # Configuration system
│   │   │   └── PolyxCmdline.[ch]   # Command line processing
│   │   └── Peerx/    # Peer services implementation
│   │       ├── PeerxMemKV.[ch]     # Memory key-value store
│   │       ├── PeerxRinetd.[ch]    # Network port forwarding
│   │       └── PeerxSqlite.[ch]    # SQLite database service
│   └── ppx.c     # Main entry point
├── test/         # Test files
│   ├── arch/     # Architecture tests
│   └── black/    # Black box tests
└── vendor/       # Third-party dependencies
```

## Core Components

### 1. InfraxCore

The foundation layer providing basic system services with a unified interface approach. This layer abstracts operating system differences and provides a consistent API.

#### Key Features:
- **Type System**: Standardized types (InfraxI8/U8/I16/U16, etc.) for platform independence
- **Memory Management**: Allocation, reallocation, and deallocation with tracking
- **Thread Management**: Creation, synchronization, and lifecycle management
- **Logging System**: Configurable logging with multiple levels and outputs
- **Network Operations**: Socket creation, connection management, data transfer
- **Synchronization Primitives**: Mutexes, condition variables, semaphores
- **Error Handling**: Standardized error reporting mechanism

#### Design Pattern:
- Implements singleton pattern for global system access
- Uses function pointer tables for C-style object orientation
- Provides clean separation between interface and implementation

#### Core API Example:
```c
// Getting the InfraxCore singleton
InfraxCore* core = InfraxCoreClass.singleton();

// Memory allocation
void* ptr = core->malloc(core, size);

// Logging
core->log(core, INFRAX_LOG_INFO, "Message: %s", message);

// Error creation
InfraxError err = make_error(INFRAX_ERROR_IO, "Failed to open file");
```

### 2. PolyxScript

A lightweight scripting engine inspired by LISP but with a verb-first syntax style, designed specifically for configuration and service management.

#### Design Goals:
- Simple, intuitive syntax with verb-first style
- Seamless system integration capabilities
- Data flow processing through pipe operations
- Asynchronous programming support
- Unified expression model

#### Core Features:
- **Basic Verb System**: Fundamental operations (let/fn/expr/return)
- **Control Flow**: Conditional and loop structures (if/while/for)
- **System Integration**: Platform interoperation (pipe/bind/async/await)
- **Dynamic Type System**: Flexible data handling
- **AST Interpreter**: Efficient expression evaluation

#### Implementation Details:
- Lexical analysis with token classification
- Recursive descent parsing
- AST-based interpretation
- Dynamic typing system
- Simple symbol table for variables and functions

#### Syntax Example:
```
// Variable assignment
let(x 10)

// Function definition
fn(add [a b] expr(+ a b))

// Function call with pipe
get("/some/key") | transform() | set("/new/key")

// Asynchronous operation
async(fetch("http://example.com")) | await() | process()
```

### 3. PeerxServices

Implementation of various proxy services that expose functionality over the network using a common service interface pattern.

#### Common Service Features:
- Uniform initialization and lifecycle management
- Standard error handling and status reporting
- Configuration validation and application
- Runtime statistics collection

#### Available Services:

##### PeerxMemKV
An in-memory key-value store service supporting multiple data types and TTL-based expiration.

Features:
- Multiple value types (string, integer, float, binary)
- Time-To-Live (TTL) expiration mechanism
- Basic operations (set/get/del/exists)
- Extended operations (set with TTL, batch operations)
- Pattern-based key operations

API Example:
```c
// Create instance and initialize
PeerxMemKV* memkv = PeerxMemKVClass.new();
PeerxMemKVClass.init(memkv, config);

// Set a string value
peerx_memkv_value_t value;
peerx_memkv_value_init(&value);
value.type = PEERX_MEMKV_TYPE_STRING;
value.value.str = "value";
memkv->set(memkv, "key", &value);

// Get a value
memkv->get(memkv, "key", &value);
```

##### PeerxRinetd
A network port forwarding service that redirects traffic between ports/hosts.

Features:
- Port forwarding rule management
- Rule enable/disable control
- Traffic statistics monitoring
- Connection tracking

##### PeerxSqlite
A service providing SQLite database access over the network.

Features:
- SQL query execution
- Transaction support
- Result set streaming
- Database management operations

## Architecture Integration Layer

The PPX architecture is unified through the architecture integration layer, which provides:

1. **Initialization Sequence**:
   - PpxArch → PpxInfra → Polyx components → Peerx services

2. **Component Relationships**:
   ```
   arch→[infra+poly+peer]
   infra→[core+log+memory+thread+socket+sync+async]
   poly→[async+ds+script]
   peer→[service+plugin]
   ```

3. **Design Advantages**:
   - Clear layered structure
   - Excellent extensibility
   - Unified interface standards
   - Plugin architecture support

4. **Future Directions**:
   - Plugin system interfaces ready for implementation
   - Script engine extensibility
   - Dynamic service loading
   - Configuration hot reloading

## Design Patterns

PPX employs several design patterns to achieve its architectural goals:

### 1. Singleton Pattern
Used for global system components like InfraxCore to provide centralized access with controlled instantiation.

### 2. Object-Oriented C
Implements OOP concepts in C through:
- Struct-based "classes" with function pointers as methods
- Global "class" instances containing constructor/destructor and methods
- Inheritance through struct embedding
- Polymorphism via function pointer overriding

Example pattern:
```c
// "Class" definition
typedef struct SomeClass {
    // Fields
    int field1;
    
    // Methods
    void (*method1)(struct SomeClass* self);
} SomeClass;

// "Class type" containing constructor/static methods
typedef struct SomeClassType {
    SomeClass* (*new)(void);
    void (*free)(SomeClass* self);
    
    // Static methods
    void (*static_method)(void);
} SomeClassType;

// Global class instance
extern const SomeClassType SomeClassClass;
```

### 3. Service Pattern
Standardizes service lifecycle and management through consistent interface:
- Initialization → Start → Operation → Stop → Cleanup
- Uniform configuration handling
- Standard error reporting

## Configuration

The system uses several configuration files, each with a specific purpose:

### `ppdb.conf`: Main Configuration
Defines service endpoints and their backend storage:

```
# Format: bind_addr bind_port service_type service_params
127.0.0.1  8018  127.0.0.1 8080 # rinetd forwarding
127.0.0.1 5433 sqlite file::memory:?cache=shared # SQLite service
127.0.0.1 11211 memkv sqlite::memory:?cache=shared # MemKV service
127.0.0.1 15432 diskv sqlite:./diskv.db # DiskV service
```

### `memkv.conf`: Memory KV Settings
```
# Memory limits and behavior
max_memory 512MB
eviction_policy lru
persistence_enabled true
persistence_interval 300
```

### `sqlite3.conf`: SQLite Database Settings
```
# Database configuration
journal_mode WAL
synchronous NORMAL
temp_store MEMORY
mmap_size 268435456
cache_size 4000
busy_timeout 5000
```

### `rinetd.conf`: Port Forwarding Rules
```
# Source and destination format
# bindaddress bindport connectaddress connectport
127.0.0.1 8018 127.0.0.1 8080
127.0.0.1 8019 example.com 80
```

## Testing

The project includes a comprehensive testing strategy:

### 1. Architecture Tests (`test/arch/`)
- **Unit Tests**: Verify individual component functionality
  - Test coverage for all public APIs
  - Boundary condition testing
  - Error handling verification
- **Integration Tests**: Verify interaction between components
  - Data flow between layers
  - Service initialization sequences
  - Error propagation

### 2. Black Box Tests (`test/black/`)
- **Functional Tests**: End-to-end testing of services
  - Request/response validation
  - Protocol compliance
  - Edge case handling
- **Performance Tests**: Measure system under load
  - Throughput benchmarks
  - Latency measurements
  - Memory usage profiling
  - Connection handling limits

## Build System

The project uses a custom build system with scripts in the `script/` directory.

### Key Components:
- `script/build.sh`: Main build script
- `script/test.sh`: Test execution script
- `script/configure.sh`: Environment configuration

### Build Artifacts:
- `bin/`: Executable files (ppx, utilities)
- `lib/`: Library files (libppx.so, libinfrax.so)
- `build/`: Intermediate build files (object files, dependencies)

### Build Process:
1. Configure environment (dependencies, compiler options)
2. Compile source files into object files
3. Link object files into libraries and executables
4. Run post-build verification

## Extending PPX

### Service Implementation Guide

To implement a custom service:

1. Create service header and implementation files:
   ```c
   // PeerxMyService.h
   #include "PeerxService.h"
   
   typedef struct PeerxMyService {
       PeerxService base;  // Inherit from base service
       
       // Service-specific methods
       InfraxError (*specific_method)(struct PeerxMyService* self, ...);
   } PeerxMyService;
   
   // Class type declaration
   typedef struct PeerxMyServiceClassType {
       // Constructor/destructor
       PeerxMyService* (*new)(void);
       void (*free)(PeerxMyService* self);
       
       // Inherited service methods
       InfraxError (*init)(PeerxMyService* self, const polyx_service_config_t* config);
       // Other inherited methods...
   } PeerxMyServiceClassType;
   
   // Global class instance
   extern const PeerxMyServiceClassType PeerxMyServiceClass;
   ```

2. Implement the service in the .c file
3. Register with the service system

### Plugin System

The architecture supports a plugin system (currently in development) that will allow:
- Dynamic loading of service implementations
- Extension of scripting capabilities
- Custom protocol handlers

## Performance Considerations

### Memory Management
- Use pooled allocators for frequent allocations
- Consider object lifecycle for timely deallocation
- Profile memory usage under load

### Concurrency
- Use appropriate synchronization primitives
- Consider lock contention in high-throughput scenarios
- Leverage async operations for I/O-bound tasks

### Network Efficiency
- Implement connection pooling
- Use appropriate buffer sizes
- Consider protocol overhead

## Refactoring Guide

### Identified Areas for Improvement

1. **Error Handling**:
   - Standardize error propagation across all components
   - Enhance error context information
   - Implement structured logging for errors

2. **Memory Management**:
   - Add reference counting for shared objects
   - Implement memory usage tracking
   - Add leak detection in debug mode

3. **Test Coverage**:
   - Increase unit test coverage (target: >80%)
   - Add more integration tests
   - Implement continuous benchmarking

4. **Performance Optimizations**:
   - Profile hot spots in each component
   - Optimize critical paths
   - Reduce lock contention

### Code Quality Metrics

When refactoring, consider maintaining or improving these metrics:
- Cyclomatic complexity < 15 per function
- Function length < 100 lines
- File length < 2000 lines
- Comment ratio > 20%

## Development Guidelines

### 1. Code Organization
- Keep service implementations in appropriate directories
- Follow existing naming conventions
- Maintain clear separation of concerns
- Use consistent error handling patterns
- Document public APIs thoroughly

### 2. Testing
- Write unit tests for new features
- Ensure black box tests cover end-to-end scenarios
- Maintain test coverage
- Test error conditions thoroughly
- Include performance testing for critical paths

### 3. Documentation
- Update relevant configuration examples
- Document new features and APIs
- Keep this document current
- Comment complex algorithms and design decisions
- Provide sample code for common operations

## Future Improvements

Areas identified for potential enhancement:
1. DuckDB Integration (planned)
2. Enhanced monitoring and metrics
3. Additional service types:
   - HTTP/REST API service
   - Message queue service
   - Distributed cache service
4. Performance optimizations:
   - Zero-copy data transfer
   - Lock-free algorithms for hot paths
   - Connection pooling enhancements
5. Enhanced scripting capabilities:
   - JIT compilation for hot code paths
   - Additional built-in functions
   - Script debugging capabilities
