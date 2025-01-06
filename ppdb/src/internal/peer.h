#ifndef PPDB_INTERNAL_PEER_H_
#define PPDB_INTERNAL_PEER_H_

#include <cosmopolitan.h>
#include <ppdb/ppdb.h>
#include "base.h"
#include "storage.h"

// Macro for unused parameters
#define PPDB_UNUSED(x) ((void)(x))

// Function declarations
ppdb_error_t ppdb_storage_get(ppdb_storage_table_t* table, const void* key, size_t key_size, void* value, size_t* value_size);
ppdb_error_t ppdb_storage_put(ppdb_storage_table_t* table, const void* key, size_t key_size, const void* value, size_t value_size);
ppdb_error_t ppdb_storage_delete(ppdb_storage_table_t* table, const void* key, size_t key_size);

//-----------------------------------------------------------------------------
// Peer Layer Types
//-----------------------------------------------------------------------------

// Protocol adapter operations
typedef struct peer_ops {
    ppdb_error_t (*create)(void** proto, void* user_data);
    void (*destroy)(void* proto);
    ppdb_error_t (*on_connect)(void* proto, ppdb_handle_t conn);
    void (*on_disconnect)(void* proto, ppdb_handle_t conn);
    ppdb_error_t (*on_data)(void* proto, ppdb_handle_t conn,
                           const uint8_t* data, size_t size);
    const char* (*get_name)(void* proto);
} peer_ops_t;

// Forward declarations
typedef struct ppdb_peer_s ppdb_peer_t;
typedef struct ppdb_peer_connection_s ppdb_peer_connection_t;
typedef struct ppdb_peer_request_s ppdb_peer_request_t;
typedef struct ppdb_peer_response_s ppdb_peer_response_t;
typedef struct ppdb_proto_handler_s ppdb_proto_handler_t;

// Request structure
typedef struct ppdb_peer_request_s {
    uint32_t type;                    // Request type
    uint32_t flags;                   // Request flags
    void* data;                       // Request data
    size_t data_size;                 // Data size
    ppdb_handle_t conn;               // Connection handle
} ppdb_peer_request_t;

// Response structure  
typedef struct ppdb_peer_response_s {
    uint32_t status;                  // Response status
    void* data;                       // Response data
    size_t data_size;                 // Data size
    ppdb_error_t error;              // Error code if any
} ppdb_peer_response_t;

// Protocol handler structure
typedef struct ppdb_proto_handler_s {
    const peer_ops_t* ops;            // Protocol operations
    void* proto_data;                 // Protocol-specific data
    const char* name;                 // Protocol name
} ppdb_proto_handler_t;

// Connection structure
typedef struct ppdb_peer_connection_s {
    ppdb_peer_t* peer;                // Parent peer instance
    ppdb_storage_table_t* storage;    // Storage table for this connection
    ppdb_proto_handler_t* handler;    // Protocol handler
    void* proto_data;                // Protocol-specific data
    int socket;                      // Connection socket
    bool is_active;                  // Connection state
    uint32_t timeout;                // Connection timeout
} ppdb_peer_connection_t;

// Peer configuration structure
typedef struct ppdb_peer_config {
    uint16_t port;                     // Listening port
    uint32_t max_connections;          // Maximum number of connections
    uint32_t connection_timeout;       // Connection timeout in seconds
    uint32_t read_timeout;            // Read timeout in seconds
    uint32_t write_timeout;           // Write timeout in seconds
} ppdb_peer_config_t;

// Connection pool structure
typedef struct ppdb_conn_pool {
    ppdb_peer_t* peer;                // Parent peer instance
    uint32_t max_connections;         // Maximum number of connections
    uint32_t active_connections;      // Current active connections
    ppdb_peer_connection_t* conns;    // Array of connections
    ppdb_base_mutex_t* mutex;         // Pool mutex for thread safety
} ppdb_conn_pool_t;

// Connection state structure
typedef struct ppdb_conn_state {
    void* proto;                    // Protocol instance
    const peer_ops_t* ops;          // Protocol operations
    void* user_data;                // User data
    bool connected;                 // Connection status
    int fd;                        // Socket file descriptor
    char read_buf[4096];           // Read buffer
    size_t read_pos;               // Read position
    char write_buf[4096];          // Write buffer
    size_t write_pos;              // Write position
    ppdb_storage_table_t* storage; // Storage table for this connection
    ppdb_peer_t* peer;            // Parent peer instance
} ppdb_conn_state_t;

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

// Connection operations
ppdb_error_t ppdb_conn_create(ppdb_handle_t* conn, const peer_ops_t* ops, void* user_data);
void ppdb_conn_destroy(ppdb_handle_t conn);
ppdb_error_t ppdb_conn_set_socket(ppdb_handle_t conn, int fd);
ppdb_error_t ppdb_conn_write(ppdb_handle_t conn, const uint8_t* data, size_t size);
ppdb_error_t ppdb_conn_send(ppdb_handle_t conn, const void* data, size_t size);
ppdb_error_t ppdb_conn_recv(ppdb_handle_t conn, void* data, size_t size);
void ppdb_conn_close(ppdb_handle_t conn);
bool ppdb_conn_is_connected(ppdb_handle_t conn);
const char* ppdb_conn_get_proto_name(ppdb_handle_t conn);

#endif // PPDB_INTERNAL_PEER_H_ 