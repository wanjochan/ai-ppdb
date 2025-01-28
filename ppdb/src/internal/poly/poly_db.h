#ifndef POLY_DB_H
#define POLY_DB_H

#include "internal/infra/infra_error.h"

// Forward declarations
struct poly_db;
typedef struct poly_db poly_db_t;

struct poly_db_result;
typedef struct poly_db_result poly_db_result_t;

// Database type enumeration
typedef enum poly_db_type {
    POLY_DB_TYPE_UNKNOWN = 0,
    POLY_DB_TYPE_SQLITE = 1,    // SQLite storage engine
    POLY_DB_TYPE_DUCKDB = 2,    // DuckDB storage engine
    POLY_DB_TYPE_COUNT          // Number of database types
} poly_db_type_t;

// Database configuration
typedef struct poly_db_config {
    poly_db_type_t type;        // Database type
    const char* url;            // Database URL
    size_t max_memory;          // Maximum memory usage (0 for unlimited)
    bool read_only;             // Open in read-only mode
} poly_db_config_t;

// Database interface functions
infra_error_t poly_db_open(const poly_db_config_t* config, poly_db_t** db);
void poly_db_close(poly_db_t* db);
infra_error_t poly_db_exec(poly_db_t* db, const char* sql);
infra_error_t poly_db_query(poly_db_t* db, const char* sql, poly_db_result_t** result);
void poly_db_result_free(poly_db_result_t* result);
infra_error_t poly_db_result_row_count(poly_db_result_t* result, size_t* count);
infra_error_t poly_db_result_get_blob(poly_db_result_t* result, size_t row, size_t col, void** data, size_t* size);
infra_error_t poly_db_result_get_string(poly_db_result_t* result, size_t row, size_t col, char** str);

#endif // POLY_DB_H
