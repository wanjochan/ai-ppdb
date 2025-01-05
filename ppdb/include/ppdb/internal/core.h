#ifndef PPDB_INTERNAL_CORE_H
#define PPDB_INTERNAL_CORE_H

#include "base.h"

// Transaction status
typedef enum {
    PPDB_TXN_ACTIVE = 0,
    PPDB_TXN_COMMITTED = 1,
    PPDB_TXN_ABORTED = 2
} ppdb_txn_status_t;

// Transaction isolation level
typedef enum {
    PPDB_ISOLATION_READ_UNCOMMITTED = 0,
    PPDB_ISOLATION_READ_COMMITTED = 1,
    PPDB_ISOLATION_REPEATABLE_READ = 2,
    PPDB_ISOLATION_SERIALIZABLE = 3
} ppdb_isolation_level_t;

// Transaction descriptor
typedef struct ppdb_txn_s {
    uint64_t txn_id;
    ppdb_txn_status_t status;
    ppdb_isolation_level_t isolation;
    uint64_t start_ts;
    uint64_t commit_ts;
    ppdb_core_mutex_t* mutex;
    struct ppdb_txn_s* next;
} ppdb_txn_t;

// MVCC version
typedef struct ppdb_version_s {
    uint64_t txn_id;
    uint64_t ts;
    ppdb_value_t value;
    struct ppdb_version_s* next;
} ppdb_version_t;

// MVCC key-value pair
typedef struct {
    ppdb_key_t key;
    ppdb_version_t* versions;
    ppdb_core_mutex_t* mutex;
} ppdb_mvcc_item_t;

// Core context
typedef struct {
    ppdb_base_t* base;
    ppdb_core_mutex_t* txn_mutex;
    ppdb_txn_t* active_txns;
    uint64_t next_txn_id;
    uint64_t next_ts;
} ppdb_core_t;

// Core initialization and cleanup
ppdb_error_t ppdb_core_init(ppdb_core_t** core, ppdb_base_t* base);
void ppdb_core_destroy(ppdb_core_t* core);

// Transaction management
ppdb_error_t ppdb_core_txn_begin(ppdb_core_t* core, ppdb_isolation_level_t isolation, ppdb_txn_t** txn);
ppdb_error_t ppdb_core_txn_commit(ppdb_core_t* core, ppdb_txn_t* txn);
ppdb_error_t ppdb_core_txn_abort(ppdb_core_t* core, ppdb_txn_t* txn);

// MVCC operations
ppdb_error_t ppdb_core_get(ppdb_core_t* core, ppdb_txn_t* txn, const ppdb_key_t* key, ppdb_value_t* value);
ppdb_error_t ppdb_core_put(ppdb_core_t* core, ppdb_txn_t* txn, const ppdb_key_t* key, const ppdb_value_t* value);
ppdb_error_t ppdb_core_delete(ppdb_core_t* core, ppdb_txn_t* txn, const ppdb_key_t* key);

#endif // PPDB_INTERNAL_CORE_H 