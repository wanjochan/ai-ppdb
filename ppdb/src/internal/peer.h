#ifndef PPDB_PEER_H_
#define PPDB_PEER_H_

#include <cosmopolitan.h>
#include "base.h"
#include "database.h"

// Connection state
typedef struct ppdb_conn_state {
    ppdb_ctx_t* ctx;                // Database context
    ppdb_database_txn_t* txn;       // Current transaction
    ppdb_database_table_t* table;   // Current table
    int socket;                     // Socket descriptor
} ppdb_conn_state_t;

// Connection handle
typedef struct ppdb_handle {
    ppdb_ctx_t* ctx;                // Database context
    ppdb_database_txn_t* txn;       // Current transaction
    ppdb_database_table_t* table;   // Current table
    ppdb_conn_state_t* state;       // Connection state
} ppdb_handle_t;

// ... rest of existing code ... 