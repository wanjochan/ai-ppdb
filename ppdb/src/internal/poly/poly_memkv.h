#ifndef POLY_MEMKV_H
#define POLY_MEMKV_H

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/poly/poly_db.h"

// Memory KV engine type
typedef enum poly_memkv_engine {
    POLY_MEMKV_ENGINE_SQLITE,  // Use SQLite as backend
    POLY_MEMKV_ENGINE_DUCKDB  // Use DuckDB as backend
} poly_memkv_engine_t;

// Memory KV database handle
typedef struct poly_memkv_db {
    poly_memkv_engine_t engine;  // Engine type
    poly_db_t* db;              // Underlying database interface
    void* impl;                 // Implementation handle (poly_sqlitekv_db_t or poly_duckdbkv_db_t)
} poly_memkv_db_t;

// Memory KV iterator
typedef struct poly_memkv_iter {
    poly_memkv_engine_t engine;  // Engine type
    poly_db_result_t* result;    // Underlying query result
    void* impl;                  // Implementation iterator
    size_t current_row;          // Current row
    size_t total_rows;           // Total rows
} poly_memkv_iter_t;

// Error codes specific to Memory KV
typedef enum poly_memkv_error {
    POLY_MEMKV_ERROR_NONE = 0,
    POLY_MEMKV_ERROR_INVALID_CONFIG = -1,
    POLY_MEMKV_ERROR_INVALID_ENGINE = -2,
    POLY_MEMKV_ERROR_KEY_TOO_LARGE = -3,
    POLY_MEMKV_ERROR_VALUE_TOO_LARGE = -4,
    POLY_MEMKV_ERROR_KEY_NOT_FOUND = -5,
    POLY_MEMKV_ERROR_MEMORY_LIMIT = -6,
    POLY_MEMKV_ERROR_INTERNAL = -7
} poly_memkv_error_t;

// Configuration for memory KV store
typedef struct poly_memkv_config {
    poly_memkv_engine_t engine;    // Engine type
    const char* url;               // Database URL (replaces path)
    size_t max_key_size;          // Maximum key size
    size_t max_value_size;        // Maximum value size
    size_t memory_limit;          // Maximum memory usage (0 for unlimited)
    bool enable_compression;      // Enable value compression
} poly_memkv_config_t;

// Interface functions
infra_error_t poly_memkv_create(const poly_memkv_config_t* config, poly_memkv_db_t** db);
void poly_memkv_destroy(poly_memkv_db_t* db);
infra_error_t poly_memkv_get(poly_memkv_db_t* db, const char* key, void** value, size_t* value_len);
infra_error_t poly_memkv_set(poly_memkv_db_t* db, const char* key, const void* value, size_t value_len);
infra_error_t poly_memkv_del(poly_memkv_db_t* db, const char* key);
infra_error_t poly_memkv_exec(poly_memkv_db_t* db, const char* sql);

// Iterator functions
infra_error_t poly_memkv_iter_create(poly_memkv_db_t* db, poly_memkv_iter_t** iter);
infra_error_t poly_memkv_iter_next(poly_memkv_iter_t* iter, char** key, void** value, size_t* value_len);
void poly_memkv_iter_destroy(poly_memkv_iter_t* iter);

#endif // POLY_MEMKV_H 
