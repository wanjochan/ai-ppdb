#ifndef PPDB_INTERNAL_STORAGE_H_
#define PPDB_INTERNAL_STORAGE_H_

#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/engine.h"

//-----------------------------------------------------------------------------
// Storage layer types
//-----------------------------------------------------------------------------

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

// Default values
#define PPDB_DEFAULT_DATA_DIR      "data"

// Forward declarations
typedef struct ppdb_storage_s ppdb_storage_t;
typedef struct ppdb_storage_table_s ppdb_storage_table_t;
typedef struct ppdb_storage_index_s ppdb_storage_index_t;

// Internal functions
static inline int data_compare(const void* a, const void* b) {
    size_t size_a = *(const size_t*)a;
    size_t size_b = *(const size_t*)b;
    const char* data_a = (const char*)a + sizeof(size_t);
    const char* data_b = (const char*)b + sizeof(size_t);
    size_t min_size = size_a < size_b ? size_a : size_b;
    int result = memcmp(data_a, data_b, min_size);
    if (result == 0) {
        return size_a - size_b;
    }
    return result;
}

// Maintenance structure
typedef struct ppdb_storage_maintain_s {
    ppdb_base_mutex_t* mutex;      // Maintenance mutex
    ppdb_base_thread_t* thread;    // Maintenance thread
    bool is_running;               // Maintenance thread state
    bool should_stop;              // Stop flag
} ppdb_storage_maintain_t;

// Table structure
struct ppdb_storage_table_s {
    char* name;                    // Table name
    ppdb_base_skiplist_t* data;    // Table data
    ppdb_base_skiplist_t* indexes; // Table indexes
    ppdb_base_spinlock_t lock;     // Table lock
    uint64_t size;                 // Number of records
    bool is_open;                  // Table open state
};

// Cursor type
typedef struct ppdb_storage_cursor_s {
    ppdb_storage_table_t* table;
    ppdb_base_skiplist_node_t* current;
    bool valid;
} ppdb_storage_cursor_t;

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
    ppdb_storage_maintain_t maintain;    // Maintenance structure
};

//-----------------------------------------------------------------------------
// Storage layer functions
//-----------------------------------------------------------------------------

// Storage initialization and cleanup
ppdb_error_t ppdb_storage_init(ppdb_storage_t** storage, ppdb_base_t* base, const ppdb_storage_config_t* config);
void ppdb_storage_destroy(ppdb_storage_t* storage);
void ppdb_storage_get_stats(ppdb_storage_t* storage, ppdb_storage_stats_t* stats);

// Table operations
ppdb_error_t ppdb_table_create(ppdb_storage_t* storage, const char* name);
ppdb_error_t ppdb_table_drop(ppdb_storage_t* storage, const char* name);
ppdb_error_t ppdb_table_open(ppdb_storage_t* storage, const char* name);
ppdb_error_t ppdb_table_close(ppdb_storage_t* storage);
ppdb_error_t ppdb_storage_get_table(ppdb_storage_t* storage, const char* name, ppdb_storage_table_t** table);
ppdb_error_t ppdb_storage_create_table(ppdb_storage_t* storage, const char* name, ppdb_storage_table_t** table);
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

// Table management functions
ppdb_error_t ppdb_storage_table_create(ppdb_storage_t* storage, const char* name, ppdb_storage_table_t** table);
void ppdb_storage_table_destroy(ppdb_storage_table_t* table);

// Configuration management
ppdb_error_t ppdb_storage_config_init(ppdb_storage_config_t* config);
ppdb_error_t ppdb_storage_config_validate(const ppdb_storage_config_t* config);
ppdb_error_t ppdb_storage_get_config(ppdb_storage_t* storage, ppdb_storage_config_t* config);
ppdb_error_t ppdb_storage_update_config(ppdb_storage_t* storage, const ppdb_storage_config_t* config);

#endif // PPDB_INTERNAL_STORAGE_H_ 