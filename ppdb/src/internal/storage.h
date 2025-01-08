#ifndef PPDB_STORAGE_H
#define PPDB_STORAGE_H

#include "base.h"
#include "engine.h"

// Default configuration values
#define PPDB_DEFAULT_MEMTABLE_SIZE     (64 * 1024 * 1024)  // 64MB
#define PPDB_DEFAULT_BLOCK_SIZE        (4 * 1024)          // 4KB
#define PPDB_DEFAULT_CACHE_SIZE        (256 * 1024 * 1024) // 256MB
#define PPDB_DEFAULT_WRITE_BUFFER_SIZE (8 * 1024 * 1024)   // 8MB
#define PPDB_DEFAULT_DATA_DIR          "data"
#define PPDB_DEFAULT_USE_COMPRESSION   true
#define PPDB_DEFAULT_SYNC_WRITES       false

// Error codes (4400-4599)
#define PPDB_STORAGE_ERR_START         4400
#define PPDB_STORAGE_ERR_PARAM         4401
#define PPDB_STORAGE_ERR_MEMORY        4402
#define PPDB_STORAGE_ERR_NOT_FOUND     4403
#define PPDB_STORAGE_ERR_DUPLICATE     4404
#define PPDB_STORAGE_ERR_INVALID_STATE 4405
#define PPDB_STORAGE_ERR_TXN_CONFLICT  4406
#define PPDB_STORAGE_ERR_BUFFER_FULL   4407
#define PPDB_STORAGE_ERR_TABLE         4408
#define PPDB_STORAGE_ERR_INDEX         4409
#define PPDB_STORAGE_ERR_WAL           4410
#define PPDB_STORAGE_ERR_IO            4411
#define PPDB_STORAGE_ERR_CONFIG        4412
#define PPDB_STORAGE_ERR_INTERNAL      4413
#define PPDB_STORAGE_ERR_TABLE_EXISTS  4414
#define PPDB_STORAGE_ERR_TABLE_NOT_FOUND 4415

// Forward declarations
typedef struct ppdb_storage_s ppdb_storage_t;
typedef struct ppdb_storage_table_s ppdb_storage_table_t;
typedef struct ppdb_storage_maintain_s ppdb_storage_maintain_t;
typedef struct ppdb_storage_config_s ppdb_storage_config_t;
typedef struct ppdb_storage_stats_s ppdb_storage_stats_t;
typedef struct ppdb_storage_cursor_s ppdb_storage_cursor_t;

// Storage configuration
typedef struct ppdb_storage_config_s {
    size_t memtable_size;      // Size of memtable in bytes
    size_t block_size;         // Size of data blocks in bytes
    size_t cache_size;         // Size of block cache in bytes
    size_t write_buffer_size;  // Size of write buffer in bytes
    const char* data_dir;      // Directory for data files
    bool use_compression;      // Whether to use compression
    bool sync_writes;          // Whether to sync writes to disk
    bool use_checksum;         // Whether to use checksums
    size_t max_file_size;      // Maximum size of data files
    size_t max_open_files;     // Maximum number of open files
} ppdb_storage_config_t;

// Storage statistics
typedef struct ppdb_storage_stats_s {
    uint64_t total_reads;      // Total number of reads
    uint64_t total_writes;     // Total number of writes
    uint64_t cache_hits;       // Number of cache hits
    uint64_t cache_misses;     // Number of cache misses
    uint64_t bytes_written;    // Total bytes written
    uint64_t bytes_read;       // Total bytes read
    uint64_t compactions;      // Number of compactions
    uint64_t flushes;         // Number of flushes
    uint64_t wal_syncs;       // Number of WAL syncs
} ppdb_storage_stats_t;

// Storage cursor for scanning
typedef struct ppdb_storage_cursor_s {
    ppdb_storage_table_t* table;
    ppdb_engine_txn_t* txn;
    ppdb_engine_cursor_t* engine_cursor;
    void* key;
    size_t key_size;
    void* value;
    size_t value_size;
    bool valid;
    bool reverse;
} ppdb_storage_cursor_t;

// Storage maintenance structure
struct ppdb_storage_maintain_s {
    ppdb_base_mutex_t* lock;
    bool is_running;
    bool should_stop;
    ppdb_base_async_handle_t* task;
    uint64_t last_compact_time;
    uint64_t last_cleanup_time;
    uint64_t last_optimize_time;
};

// Storage table structure
struct ppdb_storage_table_s {
    char* name;
    size_t name_len;
    ppdb_engine_table_t* engine_table;
    ppdb_engine_t* engine;
    ppdb_storage_t* storage;
    size_t size;
    bool is_open;
    ppdb_base_mutex_t* lock;
    uint64_t last_access_time;
    uint64_t last_modify_time;
};

// Storage structure
struct ppdb_storage_s {
    ppdb_engine_t* engine;
    ppdb_base_mutex_t* lock;
    ppdb_engine_txn_t* current_tx;
    ppdb_engine_table_list_t* tables;
    ppdb_storage_config_t config;
    ppdb_storage_stats_t stats;
    ppdb_storage_maintain_t* maintain;
    bool initialized;
    bool shutting_down;
};

// Storage functions
ppdb_error_t ppdb_storage_init(ppdb_storage_t** storage, ppdb_engine_t* engine, const ppdb_storage_config_t* config);
void ppdb_storage_destroy(ppdb_storage_t* storage);

// Table functions
ppdb_error_t ppdb_storage_create_table(ppdb_storage_t* storage, const void* name_key, ppdb_storage_table_t** table);
ppdb_error_t ppdb_storage_drop_table(ppdb_storage_t* storage, const void* name_key);
ppdb_error_t ppdb_storage_get_table(ppdb_storage_t* storage, const void* name_key, ppdb_storage_table_t** table);
void ppdb_storage_table_destroy(ppdb_storage_table_t* table);

// Data operations
ppdb_error_t ppdb_storage_put(ppdb_storage_table_t* table,
                             const void* key, size_t key_size,
                             const void* value, size_t value_size);
ppdb_error_t ppdb_storage_get(ppdb_storage_table_t* table,
                             const void* key, size_t key_size,
                             void* value, size_t* value_size);
ppdb_error_t ppdb_storage_delete(ppdb_storage_table_t* table,
                                const void* key, size_t key_size);

// Cursor operations
ppdb_error_t ppdb_storage_cursor_create(ppdb_storage_table_t* table,
                                       ppdb_storage_cursor_t** cursor);
void ppdb_storage_cursor_destroy(ppdb_storage_cursor_t* cursor);
ppdb_error_t ppdb_storage_cursor_seek(ppdb_storage_cursor_t* cursor,
                                     const void* key, size_t key_size);
ppdb_error_t ppdb_storage_cursor_next(ppdb_storage_cursor_t* cursor);
ppdb_error_t ppdb_storage_cursor_prev(ppdb_storage_cursor_t* cursor);
bool ppdb_storage_cursor_valid(ppdb_storage_cursor_t* cursor);
ppdb_error_t ppdb_storage_cursor_key(ppdb_storage_cursor_t* cursor,
                                    void* key, size_t* key_size);
ppdb_error_t ppdb_storage_cursor_value(ppdb_storage_cursor_t* cursor,
                                      void* value, size_t* value_size);

// Statistics functions
void ppdb_storage_get_stats(ppdb_storage_t* storage, ppdb_storage_stats_t* stats);
ppdb_error_t ppdb_storage_stats_init(ppdb_storage_stats_t* stats);
void ppdb_storage_stats_cleanup(ppdb_storage_stats_t* stats);

// Maintenance functions
ppdb_error_t ppdb_storage_maintain_init(ppdb_storage_t* storage);
ppdb_error_t ppdb_storage_maintain_cleanup(ppdb_storage_t* storage);
ppdb_error_t ppdb_storage_maintain_start(ppdb_storage_t* storage);
ppdb_error_t ppdb_storage_maintain_stop(ppdb_storage_t* storage);
ppdb_error_t ppdb_storage_maintain_compact(ppdb_storage_t* storage);
ppdb_error_t ppdb_storage_maintain_cleanup_expired(ppdb_storage_t* storage);
ppdb_error_t ppdb_storage_maintain_optimize_indexes(ppdb_storage_t* storage);

// Configuration functions
ppdb_error_t ppdb_storage_config_init(ppdb_storage_config_t* config);
ppdb_error_t ppdb_storage_config_validate(const ppdb_storage_config_t* config);
ppdb_error_t ppdb_storage_get_config(ppdb_storage_t* storage, ppdb_storage_config_t* config);
ppdb_error_t ppdb_storage_update_config(ppdb_storage_t* storage, const ppdb_storage_config_t* config);

// Error message conversion
const char* ppdb_storage_strerror(ppdb_error_t err);

#endif // PPDB_STORAGE_H