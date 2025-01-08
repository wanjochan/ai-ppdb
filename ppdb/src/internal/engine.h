#ifndef PPDB_INTERNAL_ENGINE_H
#define PPDB_INTERNAL_ENGINE_H

#include <cosmopolitan.h>
#include "internal/base.h"

// Forward declarations
typedef struct ppdb_engine_s ppdb_engine_t;
typedef struct ppdb_engine_txn_s ppdb_engine_txn_t;
typedef struct ppdb_engine_table_s ppdb_engine_table_t;
typedef struct ppdb_engine_cursor_s ppdb_engine_cursor_t;
typedef struct ppdb_engine_table_list_s ppdb_engine_table_list_t;

// Statistics
typedef struct ppdb_engine_stats_s {
    ppdb_base_counter_t* total_txns;
    ppdb_base_counter_t* active_txns;
    ppdb_base_counter_t* total_reads;
    ppdb_base_counter_t* total_writes;
} ppdb_engine_stats_t;

typedef struct ppdb_engine_txn_stats_s {
    ppdb_base_counter_t* reads;
    ppdb_base_counter_t* writes;
    bool is_active;
    bool is_committed;
    bool is_rolledback;
    ppdb_error_t error_state;  // Current error state of the transaction
} ppdb_engine_txn_stats_t;

// Transaction structure
struct ppdb_engine_txn_s {
    ppdb_engine_t* engine;        // Engine instance
    uint64_t id;                  // Transaction ID
    ppdb_engine_txn_stats_t stats; // Transaction statistics
    ppdb_engine_txn_t* next;      // Next transaction in list
};

// Table structure
struct ppdb_engine_table_s {
    char* name;                    // Table name
    size_t name_len;              // Length of table name
    ppdb_engine_t* engine;        // Engine instance
    ppdb_base_mutex_t* lock;      // Table lock
    uint64_t size;                // Number of records
    bool is_open;                 // Table open state
};

// Cursor structure
struct ppdb_engine_cursor_s {
    ppdb_engine_table_t* table;   // Table being scanned
    ppdb_engine_txn_t* txn;       // Transaction context
    bool valid;                   // Whether cursor points to valid data
    bool reverse;                 // Scan direction
};

// Engine layer error codes (0x1200-0x12FF)
#define PPDB_ENGINE_ERR_START   (PPDB_ERROR_START + 0x200)  // Engine: 0x1200-0x12FF
#define PPDB_ENGINE_ERR_INIT       (PPDB_ENGINE_ERR_START + 0x001)
#define PPDB_ENGINE_ERR_PARAM      (PPDB_ENGINE_ERR_START + 0x002)
#define PPDB_ENGINE_ERR_MUTEX      (PPDB_ENGINE_ERR_START + 0x003)
#define PPDB_ENGINE_ERR_TXN        (PPDB_ENGINE_ERR_START + 0x004)
#define PPDB_ENGINE_ERR_MVCC       (PPDB_ENGINE_ERR_START + 0x005)
#define PPDB_ENGINE_ERR_ASYNC      (PPDB_ENGINE_ERR_START + 0x006)
#define PPDB_ENGINE_ERR_TIMEOUT    (PPDB_ENGINE_ERR_START + 0x007)
#define PPDB_ENGINE_ERR_BUSY       (PPDB_ENGINE_ERR_START + 0x008)
#define PPDB_ENGINE_ERR_FULL       (PPDB_ENGINE_ERR_START + 0x009)
#define PPDB_ENGINE_ERR_NOT_FOUND  (PPDB_ENGINE_ERR_START + 0x00A)
#define PPDB_ENGINE_ERR_EXISTS     (PPDB_ENGINE_ERR_START + 0x00B)
#define PPDB_ENGINE_ERR_INVALID_STATE (PPDB_ENGINE_ERR_START + 0x00C)
#define PPDB_ENGINE_ERR_MEMORY     (PPDB_ENGINE_ERR_START + 0x00D)
#define PPDB_ENGINE_ERR_BUFFER_FULL (PPDB_ENGINE_ERR_START + 0x00E)

// Error message conversion function
const char* ppdb_engine_strerror(ppdb_error_t err);

// Transaction manager structure
typedef struct ppdb_engine_txn_mgr_s {
    ppdb_base_mutex_t* txn_mutex;  // Transaction manager mutex
    uint64_t next_txn_id;         // Next transaction ID
    ppdb_engine_txn_t* active_txns; // Active transactions list
} ppdb_engine_txn_mgr_t;

// IO manager structure
typedef struct ppdb_engine_io_mgr_s {
    ppdb_base_io_manager_t* io_mgr;  // IO manager instance
    ppdb_base_thread_t* io_thread;   // IO thread
    bool io_running;                 // IO thread running flag
} ppdb_engine_io_mgr_t;

// Engine structure
struct ppdb_engine_s {
    ppdb_base_t* base;            // Base layer instance
    ppdb_base_mutex_t* global_mutex; // Global mutex
    ppdb_engine_stats_t stats;    // Engine statistics
    ppdb_engine_txn_mgr_t txn_mgr; // Transaction manager
    ppdb_engine_io_mgr_t io_mgr;   // IO manager
    ppdb_engine_table_list_t* tables; // Table list
};

// Engine-specific function declarations
ppdb_error_t ppdb_engine_init(ppdb_engine_t** engine, ppdb_base_t* base);
void ppdb_engine_destroy(ppdb_engine_t* engine);

// Transaction management
ppdb_error_t ppdb_engine_txn_init(ppdb_engine_t* engine);
void ppdb_engine_txn_cleanup(ppdb_engine_t* engine);
ppdb_error_t ppdb_engine_txn_begin(ppdb_engine_t* engine, ppdb_engine_txn_t** txn);
ppdb_error_t ppdb_engine_txn_commit(ppdb_engine_txn_t* txn);
ppdb_error_t ppdb_engine_txn_rollback(ppdb_engine_txn_t* txn);

// Table operations
ppdb_error_t ppdb_engine_table_create(ppdb_engine_txn_t* txn, const char* name, ppdb_engine_table_t** table);
ppdb_error_t ppdb_engine_table_open(ppdb_engine_txn_t* txn, const char* name, ppdb_engine_table_t** table);
ppdb_error_t ppdb_engine_table_close(ppdb_engine_table_t* table);
ppdb_error_t ppdb_engine_table_drop(ppdb_engine_txn_t* txn, const char* name);
uint64_t ppdb_engine_table_size(ppdb_engine_table_t* table);

// Data operations
ppdb_error_t ppdb_engine_put(ppdb_engine_txn_t* txn, ppdb_engine_table_t* table, const void* key, size_t key_size, const void* value, size_t value_size);
ppdb_error_t ppdb_engine_get(ppdb_engine_txn_t* txn, ppdb_engine_table_t* table, const void* key, size_t key_size, void* value, size_t* value_size);
ppdb_error_t ppdb_engine_delete(ppdb_engine_txn_t* txn, ppdb_engine_table_t* table, const void* key, size_t key_size);

// Cursor operations
ppdb_error_t ppdb_engine_cursor_open(ppdb_engine_txn_t* txn, ppdb_engine_table_t* table, ppdb_engine_cursor_t** cursor);
ppdb_error_t ppdb_engine_cursor_close(ppdb_engine_cursor_t* cursor);
ppdb_error_t ppdb_engine_cursor_first(ppdb_engine_cursor_t* cursor);
ppdb_error_t ppdb_engine_cursor_last(ppdb_engine_cursor_t* cursor);
ppdb_error_t ppdb_engine_cursor_next(ppdb_engine_cursor_t* cursor);
ppdb_error_t ppdb_engine_cursor_prev(ppdb_engine_cursor_t* cursor);
ppdb_error_t ppdb_engine_cursor_seek(ppdb_engine_cursor_t* cursor, const void* key, size_t key_size);

// IO management
ppdb_error_t ppdb_engine_io_init(ppdb_engine_t* engine);
void ppdb_engine_io_cleanup(ppdb_engine_t* engine);

// Statistics functions
void ppdb_engine_get_stats(ppdb_engine_t* engine, ppdb_engine_stats_t* stats);
void ppdb_engine_txn_get_stats(ppdb_engine_txn_t* txn, ppdb_engine_txn_stats_t* stats);

// Statistics initialization and cleanup
ppdb_error_t ppdb_engine_stats_init(ppdb_engine_stats_t* stats);
void ppdb_engine_stats_cleanup(ppdb_engine_stats_t* stats);

// Maintenance operations
ppdb_error_t ppdb_engine_compact(ppdb_engine_txn_t* txn, ppdb_engine_table_t* table);
ppdb_error_t ppdb_engine_flush(ppdb_engine_txn_t* txn, ppdb_engine_table_t* table);

// Mutex operations
ppdb_error_t ppdb_engine_mutex_create(ppdb_base_mutex_t** mutex);
void ppdb_engine_mutex_destroy(ppdb_base_mutex_t* mutex);
ppdb_error_t ppdb_engine_mutex_lock(ppdb_base_mutex_t* mutex);
ppdb_error_t ppdb_engine_mutex_unlock(ppdb_base_mutex_t* mutex);

// Function types
typedef void (*ppdb_engine_async_fn)(void* arg);

// Async operations
ppdb_error_t ppdb_engine_async_schedule(ppdb_engine_t* engine, ppdb_engine_async_fn fn, void* arg, ppdb_base_async_handle_t** handle);
void ppdb_engine_async_cancel(ppdb_base_async_handle_t* handle);

// Thread operations
void ppdb_engine_yield(void);
void ppdb_engine_sleep(uint32_t milliseconds);

// Memory operations
void* ppdb_engine_malloc(size_t size);
void ppdb_engine_free(void* ptr);

// Table maintenance operations
ppdb_error_t ppdb_engine_table_compact(ppdb_engine_table_t* table);
ppdb_error_t ppdb_engine_table_cleanup_expired(ppdb_engine_table_t* table);
ppdb_error_t ppdb_engine_table_optimize_indexes(ppdb_engine_table_t* table);

// Table list management
typedef struct ppdb_engine_table_list_s {
    ppdb_base_skiplist_t* skiplist;
    ppdb_base_mutex_t* lock;
    ppdb_engine_t* engine;
} ppdb_engine_table_list_t;

// Table list operations
ppdb_error_t ppdb_engine_table_list_create(ppdb_engine_t* engine, ppdb_engine_table_list_t** list);
ppdb_error_t ppdb_engine_table_list_destroy(ppdb_engine_table_list_t* list);
ppdb_error_t ppdb_engine_table_list_add(ppdb_engine_table_list_t* list, ppdb_engine_table_t* table);
ppdb_error_t ppdb_engine_table_list_remove(ppdb_engine_table_list_t* list, const char* name);
ppdb_error_t ppdb_engine_table_list_find(ppdb_engine_table_list_t* list, const char* name, ppdb_engine_table_t** table);
ppdb_error_t ppdb_engine_table_list_foreach(ppdb_engine_table_list_t* list, void (*fn)(ppdb_engine_table_t* table, void* arg), void* arg);

#endif // PPDB_INTERNAL_ENGINE_H