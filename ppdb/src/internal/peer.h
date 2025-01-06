#ifndef PPDB_INTERNAL_PEER_H_
#define PPDB_INTERNAL_PEER_H_

#include <cosmopolitan.h>
#include <ppdb/ppdb.h>
#include "base.h"

//-----------------------------------------------------------------------------
// Peer Layer Types
//-----------------------------------------------------------------------------

// Forward declarations
typedef struct ppdb_peer_s ppdb_peer_t;
typedef struct ppdb_peer_connection_s ppdb_peer_connection_t;
typedef struct ppdb_peer_request_s ppdb_peer_request_t;
typedef struct ppdb_peer_response_s ppdb_peer_response_t;

// Protocol adapter operations
typedef struct peer_ops {
    ppdb_error_t (*create)(void** proto, void* user_data);
    void (*destroy)(void* proto);
    ppdb_error_t (*on_connect)(void* proto, ppdb_conn_t conn);
    void (*on_disconnect)(void* proto, ppdb_conn_t conn);
    ppdb_error_t (*on_data)(void* proto, ppdb_conn_t conn,
                           const uint8_t* data, size_t size);
    const char* (*get_name)(void* proto);
} peer_ops_t;

// Connection pool structure
typedef struct ppdb_conn_pool {
    // TODO: Define connection pool structure
} ppdb_conn_pool_t;

// Protocol handler structure
typedef struct ppdb_proto_handler {
    // TODO: Define protocol handler structure
} ppdb_proto_handler_t;

// Internal peer structure
struct ppdb_peer {
    ppdb_peer_config_t config;         // Peer configuration
    ppdb_conn_pool_t *conn_pool;       // Connection pool
    ppdb_proto_handler_t *proto_handlers;  // Protocol handlers
};

// Protocol adapter getters
const peer_ops_t* peer_get_memcached(void);
const peer_ops_t* peer_get_redis(void);

// Core Functions
int peer_init(void);
void peer_cleanup(void);
int peer_is_initialized(void);

#endif // PPDB_INTERNAL_PEER_H_ 