#ifndef POLYX_DB_INTERFACE_H
#define POLYX_DB_INTERFACE_H

#include "internal/infrax/InfraxCore.h"
#include "internal/polyx/PolyxService.h"

// Forward declarations
typedef struct PolyxDB PolyxDB;
typedef struct PolyxDBClassType PolyxDBClassType;

// Database types
typedef enum {
    POLYX_DB_TYPE_SQLITE,
    POLYX_DB_TYPE_DUCKDB
} polyx_db_type_t;//TODO PolyxDBType ?

// Database configuration
typedef struct {
    polyx_db_type_t type;        // Database type
    const char* url;            // Database URL
    size_t max_memory;          // Maximum memory usage (0 for unlimited)
    InfraxBool  read_only;             // Open in read-only mode
    const char* plugin_path;    // Path to dynamic library (for DuckDB)
    InfraxBool allow_fallback;        // Allow fallback to SQLite if DuckDB fails
} polyx_db_config_t;

// Query result
typedef struct {
    size_t column_count;
    char** column_names;
    char*** rows;
    size_t row_count;
} polyx_db_result_t;

// Database instance
struct PolyxDB {
    PolyxDB* self;
    const PolyxDBClassType* klass;
    
    // Private data
    void* private_data;
    
    // Database operations
    InfraxError (*open)(PolyxDB* self, const polyx_db_config_t* config);
    InfraxError (*close)(PolyxDB* self);
    InfraxError (*exec)(PolyxDB* self, const char* sql);
    InfraxError (*query)(PolyxDB* self, const char* sql, polyx_db_result_t* result);
    void (*free_result)(PolyxDB* self, polyx_db_result_t* result);
    
    // Transaction management
    InfraxError (*begin)(PolyxDB* self);
    InfraxError (*commit)(PolyxDB* self);
    InfraxError (*rollback)(PolyxDB* self);
    
    // Key-value operations
    InfraxError (*set)(PolyxDB* self, const char* key, const void* value, size_t value_size);
    InfraxError (*get)(PolyxDB* self, const char* key, void** value, size_t* value_size);
    InfraxError (*del)(PolyxDB* self, const char* key);
    InfraxBool (*exists)(PolyxDB* self, const char* key);
    
    // Status and error handling
    InfraxError (*get_status)(PolyxDB* self, char* status, size_t size);
    const char* (*get_error)(PolyxDB* self);
    void (*clear_error)(PolyxDB* self);
};

// Database class interface
struct PolyxDBClassType {
    // Constructor/Destructor
    PolyxDB* (*new)(void);
    void (*free)(PolyxDB* self);
    
    // Database operations
    InfraxError (*open)(PolyxDB* self, const polyx_db_config_t* config);
    InfraxError (*close)(PolyxDB* self);
    InfraxError (*exec)(PolyxDB* self, const char* sql);
    InfraxError (*query)(PolyxDB* self, const char* sql, polyx_db_result_t* result);
    void (*free_result)(PolyxDB* self, polyx_db_result_t* result);
    
    // Transaction management
    InfraxError (*begin)(PolyxDB* self);
    InfraxError (*commit)(PolyxDB* self);
    InfraxError (*rollback)(PolyxDB* self);
    
    // Key-value operations
    InfraxError (*set)(PolyxDB* self, const char* key, const void* value, size_t value_size);
    InfraxError (*get)(PolyxDB* self, const char* key, void** value, size_t* value_size);
    InfraxError (*del)(PolyxDB* self, const char* key);
    InfraxBool (*exists)(PolyxDB* self, const char* key);
    
    // Status and error handling
    InfraxError (*get_status)(PolyxDB* self, char* status, size_t size);
    const char* (*get_error)(PolyxDB* self);
    void (*clear_error)(PolyxDB* self);
};

// Global class instance
extern const PolyxDBClassType PolyxDBClass;

// Helper functions
void polyx_db_result_init(polyx_db_result_t* result);
void polyx_db_result_free(polyx_db_result_t* result);

#endif // POLYX_DB_INTERFACE_H 