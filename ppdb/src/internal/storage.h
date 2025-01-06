#ifndef PPDB_INTERNAL_STORAGE_H_
#define PPDB_INTERNAL_STORAGE_H_

#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/engine.h"

//-----------------------------------------------------------------------------
// Storage layer types
//-----------------------------------------------------------------------------

// Forward declarations
typedef struct ppdb_storage_s ppdb_storage_t;
typedef struct ppdb_storage_table_s ppdb_storage_table_t;
typedef struct ppdb_storage_index_s ppdb_storage_index_t;

// Storage statistics
typedef struct ppdb_storage_stats_s {
    ppdb_base_counter_t* reads;           // Total read operations
    ppdb_base_counter_t* writes;          // Total write operations
    ppdb_base_counter_t* flushes;         // Total memtable flushes
    ppdb_base_counter_t* compactions;     // Total compaction operations
    ppdb_base_counter_t* cache_hits;      // Block cache hits
    ppdb_base_counter_t* cache_misses;    // Block cache misses
    ppdb_base_counter_t* wal_syncs;       // WAL sync operations
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

// Default configuration values
#define PPDB_DEFAULT_MEMTABLE_SIZE      (16 * 1024 * 1024)  // 16MB
#define PPDB_DEFAULT_BLOCK_SIZE         (4 * 1024)          // 4KB
#define PPDB_DEFAULT_CACHE_SIZE         (64 * 1024 * 1024)  // 64MB
#define PPDB_DEFAULT_WRITE_BUFFER_SIZE  (4 * 1024 * 1024)   // 4MB
#define PPDB_DEFAULT_USE_COMPRESSION    true
#define PPDB_DEFAULT_SYNC_WRITES        false

// Internal storage structure
struct ppdb_storage_s {
    ppdb_base_t* base;                  // Base layer instance
    ppdb_storage_config_t config;       // Storage configuration
    ppdb_storage_stats_t stats;         // Storage statistics
    ppdb_storage_table_t* tables;       // List of tables
    ppdb_base_spinlock_t lock;          // Global storage lock
};

//-----------------------------------------------------------------------------
// Storage layer functions
//-----------------------------------------------------------------------------

// Storage initialization and cleanup
ppdb_error_t ppdb_storage_init(ppdb_storage_t** storage, ppdb_base_t* base, const ppdb_storage_config_t* config);
void ppdb_storage_destroy(ppdb_storage_t* storage);
void ppdb_storage_get_stats(ppdb_storage_t* storage, ppdb_storage_stats_t* stats);

// Table operations
ppdb_error_t ppdb_storage_create_table(ppdb_storage_t* storage, const char* table_name, ppdb_storage_table_t** table);
ppdb_error_t ppdb_storage_drop_table(ppdb_storage_t* storage, const char* table_name);
ppdb_error_t ppdb_storage_get_table(ppdb_storage_t* storage, const char* table_name, ppdb_storage_table_t** table);

// Index operations
ppdb_error_t ppdb_storage_create_index(ppdb_storage_table_t* table, const char* index_name, ppdb_storage_index_t** index);
ppdb_error_t ppdb_storage_drop_index(ppdb_storage_table_t* table, const char* index_name);
ppdb_error_t ppdb_storage_get_index(ppdb_storage_table_t* table, const char* index_name, ppdb_storage_index_t** index);

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

#endif // PPDB_INTERNAL_STORAGE_H_ 