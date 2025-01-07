#ifndef PPDB_INTERNAL_STORAGE_H_
#define PPDB_INTERNAL_STORAGE_H_

#include <cosmopolitan.h>
#include "internal/engine.h"

//-----------------------------------------------------------------------------
// Storage layer types
//-----------------------------------------------------------------------------

// Key-value types
typedef struct ppdb_storage_key_s {
    size_t size;
    char data[];
} ppdb_storage_key_t;

typedef struct ppdb_storage_value_s {
    size_t size;
    char data[];
} ppdb_storage_value_t;

// Error codes
#define PPDB_STORAGE_ERR_START     (PPDB_ERROR_START + 0x300)  // Storage: 0x1300-0x13FF
#define PPDB_STORAGE_ERR_PARAM     (PPDB_STORAGE_ERR_START + 0x01)
#define PPDB_STORAGE_ERR_TABLE     (PPDB_STORAGE_ERR_START + 0x02)
#define PPDB_STORAGE_ERR_INDEX     (PPDB_STORAGE_ERR_START + 0x03)
#define PPDB_STORAGE_ERR_WAL       (PPDB_STORAGE_ERR_START + 0x04)
#define PPDB_STORAGE_ERR_IO        (PPDB_STORAGE_ERR_START + 0x05)
#define PPDB_STORAGE_ERR_ALREADY_RUNNING (PPDB_STORAGE_ERR_START + 0x06)
#define PPDB_STORAGE_ERR_NOT_RUNNING     (PPDB_STORAGE_ERR_START + 0x07)
#define PPDB_STORAGE_ERR_TABLE_EXISTS    (PPDB_STORAGE_ERR_START + 0x08)
#define PPDB_STORAGE_ERR_TABLE_NOT_FOUND (PPDB_STORAGE_ERR_START + 0x09)
#define PPDB_STORAGE_ERR_CONFIG         (PPDB_STORAGE_ERR_START + 0x0A)
#define PPDB_STORAGE_ERR_MEMORY         (PPDB_STORAGE_ERR_START + 0x0B)
#define PPDB_STORAGE_ERR_INTERNAL       (PPDB_STORAGE_ERR_START + 0x0C)
#define PPDB_STORAGE_ERR_NOT_FOUND      (PPDB_STORAGE_ERR_START + 0x0D)
#define PPDB_STORAGE_ERR_INVALID_STATE  (PPDB_STORAGE_ERR_START + 0x0E)

// Default values
#define PPDB_DEFAULT_DATA_DIR      "data"

// Forward declarations
typedef struct ppdb_storage_s ppdb_storage_t;
typedef struct ppdb_storage_table_s ppdb_storage_table_t;
typedef struct ppdb_storage_index_s ppdb_storage_index_t;

// Engine layer types
typedef struct ppdb_engine_table_s ppdb_engine_table_t;
typedef struct ppdb_engine_cursor_s ppdb_engine_cursor_t;

// Table structure
struct ppdb_storage_table_s {
    char* name;                    // Owned table name string
    size_t name_len;              // Length of table name
    ppdb_engine_table_t* engine_table; // Engine table handle
    ppdb_engine_t* engine;        // Engine instance
    uint64_t size;                // Number of records
    bool is_open;                 // Table open state
};

// Cursor type
typedef struct ppdb_storage_cursor_s {
    ppdb_storage_table_t* table;
    ppdb_engine_cursor_t* engine_cursor;
    bool valid;
} ppdb_storage_cursor_t;

// Storage statistics
typedef struct ppdb_storage_stats_s {
    uint64_t reads;           // Total read operations
    uint64_t writes;          // Total write operations
    uint64_t flushes;         // Total memtable flushes
    uint64_t compactions;     // Total compaction operations
    uint64_t cache_hits;      // Block cache hits
    uint64_t cache_misses;    // Block cache misses
    uint64_t wal_syncs;       // WAL sync operations
} ppdb_storage_stats_t;

// Storage configuration
typedef struct ppdb_storage_config_s {
    size_t memtable_size;        // Size limit for memtable before flush
    size_t block_size;           // SSTable block size
    size_t cache_size;           // Block cache size
    size_t write_buffer_size;    // Write buffer size for WAL
    const char* data_dir;        // Data directory path
    bool use_compression;        // Whether to use compression
    bool sync_writes;            // Whether to sync writes to disk
} ppdb_storage_config_t;

// Maintenance structure
typedef struct ppdb_storage_maintain_s {
    ppdb_base_mutex_t* mutex;
    bool is_running;
    bool should_stop;
    ppdb_base_async_handle_t* task;
} ppdb_storage_maintain_t;

// Default configuration values
#define PPDB_DEFAULT_MEMTABLE_SIZE      (16 * 1024 * 1024)  // 16MB
#define PPDB_DEFAULT_BLOCK_SIZE         (4 * 1024)          // 4KB
#define PPDB_DEFAULT_CACHE_SIZE         (64 * 1024 * 1024)  // 64MB
#define PPDB_DEFAULT_WRITE_BUFFER_SIZE  (4 * 1024 * 1024)   // 4MB
#define PPDB_DEFAULT_USE_COMPRESSION    true
#define PPDB_DEFAULT_SYNC_WRITES        false

// Internal storage structure
struct ppdb_storage_s {
    ppdb_engine_t* engine;              // Engine instance
    ppdb_storage_config_t config;       // Storage configuration
    ppdb_storage_stats_t stats;         // Storage statistics
    ppdb_engine_txn_t* current_tx;      // Current transaction
    ppdb_base_mutex_t* lock;            // Global lock
    ppdb_engine_table_t* tables;        // Tables list
    ppdb_storage_maintain_t maintain;    // Maintenance info
};

//-----------------------------------------------------------------------------
// Function declarations
//-----------------------------------------------------------------------------

// Storage initialization and cleanup
ppdb_error_t ppdb_storage_init(ppdb_storage_t** storage, ppdb_engine_t* engine, const ppdb_storage_config_t* config);
void ppdb_storage_destroy(ppdb_storage_t* storage);
void ppdb_storage_get_stats(ppdb_storage_t* storage, ppdb_storage_stats_t* stats);

// Table operations
ppdb_error_t ppdb_storage_create_table(ppdb_storage_t* storage, const char* name, ppdb_storage_table_t** table);
ppdb_error_t ppdb_storage_get_table(ppdb_storage_t* storage, const void* name_key, ppdb_storage_table_t** table);
ppdb_error_t ppdb_storage_drop_table(ppdb_storage_t* storage, const char* name);

// Data operations
ppdb_error_t ppdb_storage_put(ppdb_storage_table_t* table, const void* key, size_t key_size, const void* value, size_t value_size);
ppdb_error_t ppdb_storage_get(ppdb_storage_table_t* table, const void* key, size_t key_size, void* value, size_t* value_size);
ppdb_error_t ppdb_storage_delete(ppdb_storage_table_t* table, const void* key, size_t key_size);

// Maintenance operations
ppdb_error_t ppdb_storage_flush(ppdb_storage_table_t* table);
ppdb_error_t ppdb_storage_compact(ppdb_storage_table_t* table);
ppdb_error_t ppdb_storage_backup(ppdb_storage_t* storage, const char* backup_dir);
ppdb_error_t ppdb_storage_restore(ppdb_storage_t* storage, const char* backup_dir);

// Configuration management
ppdb_error_t ppdb_storage_config_init(ppdb_storage_config_t* config);
ppdb_error_t ppdb_storage_config_validate(const ppdb_storage_config_t* config);
ppdb_error_t ppdb_storage_get_config(ppdb_storage_t* storage, ppdb_storage_config_t* config);
ppdb_error_t ppdb_storage_update_config(ppdb_storage_t* storage, const ppdb_storage_config_t* config);

// Maintenance management
ppdb_error_t ppdb_storage_maintain_init(ppdb_storage_t* storage);
void ppdb_storage_maintain_cleanup(ppdb_storage_t* storage);
ppdb_error_t ppdb_storage_maintain_start(ppdb_storage_t* storage);
ppdb_error_t ppdb_storage_maintain_stop(ppdb_storage_t* storage);
ppdb_error_t ppdb_storage_maintain_compact(ppdb_storage_t* storage);
ppdb_error_t ppdb_storage_maintain_cleanup_expired(ppdb_storage_t* storage);
ppdb_error_t ppdb_storage_maintain_optimize_indexes(ppdb_storage_t* storage);

#endif // PPDB_INTERNAL_STORAGE_H_ 