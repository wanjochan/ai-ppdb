#ifndef PPDB_INTERNAL_ENGINE_H
#define PPDB_INTERNAL_ENGINE_H

#include <cosmopolitan.h>
#include "internal/base.h"

// Common status codes
#define PPDB_OK 0

// Error codes (4000-4199)
#define PPDB_ENGINE_ERR_START        4000
#define PPDB_ENGINE_ERR_INIT         4001
#define PPDB_ENGINE_ERR_PARAM        4002
#define PPDB_ENGINE_ERR_MEMORY       4003
#define PPDB_ENGINE_ERR_NOT_FOUND    4004
#define PPDB_ENGINE_ERR_DUPLICATE    4005
#define PPDB_ENGINE_ERR_TXN_CONFLICT 4006
#define PPDB_ENGINE_ERR_IO           4007
#define PPDB_ENGINE_ERR_INTERNAL     4008
#define PPDB_ENGINE_ERR_BUSY         4009
#define PPDB_ENGINE_ERR_TIMEOUT      4010
#define PPDB_ENGINE_ERR_MUTEX        4011
#define PPDB_ENGINE_ERR_TXN          4012
#define PPDB_ENGINE_ERR_MVCC         4013
#define PPDB_ENGINE_ERR_ASYNC        4014
#define PPDB_ENGINE_ERR_FULL         4015
#define PPDB_ENGINE_ERR_EXISTS       4016
#define PPDB_ENGINE_ERR_INVALID_STATE 4017
#define PPDB_ENGINE_ERR_BUFFER_FULL  4018

// Forward declarations
typedef struct ppdb_engine_s ppdb_engine_t;
typedef struct ppdb_engine_txn_s ppdb_engine_txn_t;
typedef struct ppdb_engine_table_s ppdb_engine_table_t;
typedef struct ppdb_engine_cursor_s ppdb_engine_cursor_t;
typedef struct ppdb_engine_table_list_s ppdb_engine_table_list_t;
typedef struct ppdb_engine_rollback_record_s ppdb_engine_rollback_record_t;
typedef struct ppdb_engine_txn_stats_s ppdb_engine_txn_stats_t;
typedef struct ppdb_engine_entry_s ppdb_engine_entry_t;

// Transaction statistics
typedef struct ppdb_engine_txn_stats_s {
    uint64_t read_count;
    uint64_t write_count;
    uint64_t delete_count;
    uint64_t conflict_count;
    uint64_t rollback_count;
    uint64_t commit_count;
    uint64_t duration_ms;
} ppdb_engine_txn_stats_t;

// Entry structure
typedef struct ppdb_engine_entry_s {
    void* key;
    size_t key_size;
    void* value;
    size_t value_size;
    ppdb_engine_entry_t* next;
} ppdb_engine_entry_t;

// Table structure
struct ppdb_engine_table_s {
    char* name;
    size_t name_len;
    ppdb_engine_t* engine;
    ppdb_base_mutex_t* lock;
    ppdb_engine_entry_t* entries;
    size_t size;
    bool is_open;
};

// Cursor structure
struct ppdb_engine_cursor_s {
    ppdb_engine_table_t* table;
    ppdb_engine_txn_t* txn;
    ppdb_engine_entry_t* current;
    void* key;
    size_t key_size;
    void* value;
    size_t value_size;
    bool valid;
    bool reverse;
};

// Transaction structure
struct ppdb_engine_txn_s {
    ppdb_engine_t* engine;
    uint64_t id;
    ppdb_base_mutex_t* lock;
    ppdb_engine_txn_stats_t stats;
    ppdb_engine_rollback_record_t* rollback_records;
    size_t rollback_count;
    ppdb_engine_txn_t* next;
    bool is_write;
};

// Rollback record types
typedef enum {
    PPDB_ENGINE_ROLLBACK_PUT,
    PPDB_ENGINE_ROLLBACK_DELETE
} ppdb_engine_rollback_type_t;

// Rollback record structure
struct ppdb_engine_rollback_record_s {
    ppdb_engine_rollback_type_t type;
    ppdb_engine_table_t* table;
    void* key;
    size_t key_size;
    void* data;
    size_t value_size;
    ppdb_engine_rollback_record_t* next;
};

// Statistics
typedef struct ppdb_engine_stats_s {
    ppdb_base_counter_t* total_txns;
    ppdb_base_counter_t* active_txns;
    ppdb_base_counter_t* total_reads;
    ppdb_base_counter_t* total_writes;
} ppdb_engine_stats_t;

// Transaction manager structure
typedef struct ppdb_engine_txn_mgr_s {
    ppdb_base_mutex_t* lock;  // Transaction manager mutex
    uint64_t next_txn_id;    // Next transaction ID
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
    ppdb_base_mutex_t* lock;      // Global mutex
    ppdb_engine_stats_t stats;    // Engine statistics
    ppdb_engine_txn_mgr_t txn_mgr; // Transaction manager
    ppdb_engine_io_mgr_t io_mgr;   // IO manager
    ppdb_engine_table_list_t* tables; // Table list
};

// Engine-specific function declarations
ppdb_error_t ppdb_engine_init(ppdb_engine_t** engine, ppdb_base_t* base);
void ppdb_engine_destroy(ppdb_engine_t* engine);

// Transaction functions
ppdb_error_t ppdb_engine_txn_begin(ppdb_engine_t* engine, ppdb_engine_txn_t** txn);
ppdb_error_t ppdb_engine_txn_commit(ppdb_engine_txn_t* txn);
ppdb_error_t ppdb_engine_txn_rollback(ppdb_engine_txn_t* txn);
void ppdb_engine_txn_get_stats(ppdb_engine_txn_t* txn, ppdb_engine_txn_stats_t* stats);

// Table functions
ppdb_error_t ppdb_engine_table_create(ppdb_engine_t* engine, const char* name, ppdb_engine_table_t** table);
ppdb_error_t ppdb_engine_table_open(ppdb_engine_t* engine, const char* name, ppdb_engine_table_t** table);
void ppdb_engine_table_close(ppdb_engine_table_t* table);
ppdb_error_t ppdb_engine_table_drop(ppdb_engine_t* engine, const char* name);

// Data operations
ppdb_error_t ppdb_engine_put(ppdb_engine_txn_t* txn, ppdb_engine_table_t* table,
                            const void* key, size_t key_size,
                            const void* value, size_t value_size);
ppdb_error_t ppdb_engine_get(ppdb_engine_txn_t* txn, ppdb_engine_table_t* table,
                            const void* key, size_t key_size,
                            void* value, size_t* value_size);
ppdb_error_t ppdb_engine_delete(ppdb_engine_txn_t* txn, ppdb_engine_table_t* table,
                               const void* key, size_t key_size);

// Cursor operations
ppdb_error_t ppdb_engine_cursor_create(ppdb_engine_txn_t* txn, ppdb_engine_table_t* table,
                                      ppdb_engine_cursor_t** cursor);
void ppdb_engine_cursor_destroy(ppdb_engine_cursor_t* cursor);
ppdb_error_t ppdb_engine_cursor_seek(ppdb_engine_cursor_t* cursor,
                                    const void* key, size_t key_size);
ppdb_error_t ppdb_engine_cursor_next(ppdb_engine_cursor_t* cursor);
ppdb_error_t ppdb_engine_cursor_prev(ppdb_engine_cursor_t* cursor);
bool ppdb_engine_cursor_valid(ppdb_engine_cursor_t* cursor);
ppdb_error_t ppdb_engine_cursor_key(ppdb_engine_cursor_t* cursor,
                                   void* key, size_t* key_size);
ppdb_error_t ppdb_engine_cursor_value(ppdb_engine_cursor_t* cursor,
                                     void* value, size_t* value_size);

// Statistics functions
void ppdb_engine_get_stats(ppdb_engine_t* engine, ppdb_engine_stats_t* stats);
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
ppdb_error_t ppdb_engine_async_schedule(ppdb_engine_t* engine,
                                       ppdb_engine_async_fn fn,
                                       void* arg,
                                       ppdb_base_async_handle_t** handle);
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
struct ppdb_engine_table_list_s {
    ppdb_base_skiplist_t* skiplist;
    ppdb_base_mutex_t* lock;
    ppdb_engine_t* engine;
};

// Table list operations
ppdb_error_t ppdb_engine_table_list_create(ppdb_engine_t* engine, ppdb_engine_table_list_t** list);
void ppdb_engine_table_list_destroy(ppdb_engine_table_list_t* list);
ppdb_error_t ppdb_engine_table_list_add(ppdb_engine_table_list_t* list, ppdb_engine_table_t* table);
ppdb_error_t ppdb_engine_table_list_remove(ppdb_engine_table_list_t* list, const char* name);
ppdb_error_t ppdb_engine_table_list_find(ppdb_engine_table_list_t* list, const char* name, ppdb_engine_table_t** table);
ppdb_error_t ppdb_engine_table_list_foreach(ppdb_engine_table_list_t* list, void (*fn)(ppdb_engine_table_t* table, void* arg), void* arg);

// Error message conversion
const char* ppdb_engine_strerror(ppdb_error_t err);

#endif // PPDB_INTERNAL_ENGINE_H