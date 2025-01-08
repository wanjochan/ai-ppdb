/*
 * database.c - Database Layer Implementation
 */

#include <cosmopolitan.h>
#include "internal/database.h"

// Include implementations
#include "database/database_core.inc.c"
#include "database/database_txn.inc.c"
#include "database/database_mvcc.inc.c"
#include "database/database_memkv.inc.c"
#include "database/database_diskv.inc.c"
#include "database/database_index.inc.c"

// Database structure
struct ppdb_database_s {
    ppdb_database_table_manager_t* table_manager;
    ppdb_database_txn_manager_t* txn_manager;
    ppdb_database_index_manager_t* index_manager;
    ppdb_database_stats_t stats;
    pthread_rwlock_t rwlock;
    pthread_mutex_t mutex;
};

// Transaction structure
struct ppdb_txn_s {
    ppdb_database_t* db;
    ppdb_txn_isolation_t isolation;
    uint32_t flags;
    uint64_t txn_id;
    bool active;
};

// MVCC structure
struct ppdb_mvcc_s {
    _Atomic(uint64_t) next_txn_id;
    ppdb_base_skiplist_t* versions;
    ppdb_base_mutex_t* mutex;
};

// Index structure
struct ppdb_index_s {
    char* name;
    ppdb_base_skiplist_t* tree;
    ppdb_base_compare_func_t compare;
    ppdb_base_mutex_t* mutex;
};

// Iterator structure
struct ppdb_iterator_s {
    ppdb_txn_t* txn;
    ppdb_index_t* index;
    ppdb_base_skiplist_iterator_t* iter;
    bool valid;
}; 