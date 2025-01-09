/*
 * database.h - Database Layer Definitions
 */

#ifndef PPDB_DATABASE_H_
#define PPDB_DATABASE_H_

#include <cosmopolitan.h>
#include "base.h"

// Error codes
#define PPDB_DATABASE_ERR_START    4200
#define PPDB_DATABASE_ERR_INIT     4201  // Initialization error
#define PPDB_DATABASE_ERR_TXN      4202  // Transaction error
#define PPDB_DATABASE_ERR_CONFLICT 4203  // Conflict error
#define PPDB_DATABASE_ERR_CORRUPT  4204  // Database corruption error
#define PPDB_DATABASE_ERR_FULL     4205  // Database full error

// Forward declarations
typedef struct ppdb_database_s ppdb_database_t;
typedef struct ppdb_database_table_s ppdb_database_table_t;
typedef struct ppdb_database_txn_s ppdb_database_txn_t;
typedef struct ppdb_database_index_s ppdb_database_index_t;

// Transaction isolation levels
typedef enum {
    PPDB_TXN_READ_UNCOMMITTED = 1,
    PPDB_TXN_READ_COMMITTED = 2,
    PPDB_TXN_REPEATABLE_READ = 3,
    PPDB_TXN_SERIALIZABLE = 4
} ppdb_txn_isolation_t;

// Transaction flags
#define PPDB_TXN_READONLY  0x0001
#define PPDB_TXN_SYNC     0x0002
#define PPDB_TXN_NOWAIT   0x0004

// Database configuration
typedef struct ppdb_database_config_s {
    size_t memory_limit;           // Maximum memory usage
    size_t cache_size;            // Cache size in bytes
    bool enable_mvcc;            // Enable MVCC
    bool enable_logging;         // Enable WAL logging
    bool sync_on_commit;        // Sync to disk on commit
    ppdb_txn_isolation_t default_isolation;  // Default isolation level
    uint32_t lock_timeout_ms;   // Lock timeout in milliseconds
    uint32_t txn_timeout_ms;    // Transaction timeout in milliseconds
} ppdb_database_config_t;

// Database statistics
typedef struct ppdb_database_stats_s {
    uint64_t total_txns;         // Total transactions
    uint64_t committed_txns;     // Committed transactions
    uint64_t aborted_txns;       // Aborted transactions
    uint64_t conflicts;          // Transaction conflicts
    uint64_t deadlocks;         // Deadlocks detected
    uint64_t cache_hits;        // Cache hits
    uint64_t cache_misses;      // Cache misses
    uint64_t bytes_written;     // Total bytes written
    uint64_t bytes_read;        // Total bytes read
} ppdb_database_stats_t;

// Database functions
ppdb_error_t ppdb_database_init(ppdb_database_t** db, const ppdb_database_config_t* config);
void ppdb_database_destroy(ppdb_database_t* db);
ppdb_error_t ppdb_database_get_stats(ppdb_database_t* db, ppdb_database_stats_t* stats);

// Transaction functions
ppdb_error_t ppdb_txn_begin(ppdb_database_t* db, ppdb_txn_t** txn, uint32_t flags);
ppdb_error_t ppdb_txn_commit(ppdb_txn_t* txn);
ppdb_error_t ppdb_txn_abort(ppdb_txn_t* txn);
ppdb_error_t ppdb_txn_get_isolation(ppdb_txn_t* txn, ppdb_txn_isolation_t* isolation);
ppdb_error_t ppdb_txn_set_isolation(ppdb_txn_t* txn, ppdb_txn_isolation_t isolation);

// Index operations
ppdb_error_t ppdb_index_create(ppdb_txn_t* txn, const char* name,
                              ppdb_base_compare_func_t compare);
ppdb_error_t ppdb_index_drop(ppdb_txn_t* txn, const char* name);
ppdb_error_t ppdb_index_get(ppdb_txn_t* txn, const char* name,
                           const void* key, size_t key_size,
                           void** value, size_t* value_size);

// Iterator type and functions
typedef struct ppdb_iterator_s ppdb_iterator_t;
ppdb_error_t ppdb_iterator_create(ppdb_txn_t* txn, const char* index_name,
                                 ppdb_iterator_t** iterator);
ppdb_error_t ppdb_iterator_destroy(ppdb_iterator_t* iterator);
ppdb_error_t ppdb_iterator_seek(ppdb_iterator_t* iterator,
                               const void* key, size_t key_size);
ppdb_error_t ppdb_iterator_next(ppdb_iterator_t* iterator);
ppdb_error_t ppdb_iterator_prev(ppdb_iterator_t* iterator);
bool ppdb_iterator_valid(ppdb_iterator_t* iterator);
ppdb_error_t ppdb_iterator_key(ppdb_iterator_t* iterator,
                              void** key, size_t* key_size);
ppdb_error_t ppdb_iterator_value(ppdb_iterator_t* iterator,
                                void** value, size_t* value_size);

#endif // PPDB_DATABASE_H_ 