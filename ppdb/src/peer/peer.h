#ifndef PPDB_PEER_H
#define PPDB_PEER_H

#include <cosmopolitan.h>
#include <ppdb/ppdb.h>

//-----------------------------------------------------------------------------
// Error codes
//-----------------------------------------------------------------------------

#define PPDB_ERR_PARAM           1  // Invalid parameter
#define PPDB_ERR_MEMORY         2  // Memory allocation failed
#define PPDB_ERR_IO             3  // I/O error
#define PPDB_ERR_PROTOCOL       4  // Protocol error
#define PPDB_ERR_BUFFER_FULL    5  // Buffer full
#define PPDB_ERR_INVALID_STATE  6  // Invalid state
#define PPDB_ERR_CONNECTION_CLOSED 7  // Connection closed
#define PPDB_ERR_PARTIAL_WRITE  8  // Partial write
#define PPDB_ERR_PARTIAL_READ   9  // Partial read

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

typedef struct ppdb_peer_s ppdb_peer_t;
typedef struct ppdb_peer_connection_s ppdb_peer_connection_t;
typedef struct ppdb_conn_s* ppdb_conn_t;

typedef enum {
    PPDB_PEER_MODE_CLIENT,
    PPDB_PEER_MODE_SERVER
} ppdb_peer_mode_t;

typedef struct {
    const char* host;          // Host address
    uint16_t port;            // Port number
    uint32_t timeout_ms;      // Operation timeout
    uint32_t max_connections; // Max concurrent connections
    uint32_t io_threads;     // IO thread count
    bool use_tcp_nodelay;    // TCP_NODELAY option
    ppdb_peer_mode_t mode;   // Peer mode (client/server)
} ppdb_peer_config_t;

//-----------------------------------------------------------------------------
// Protocol adapters
//-----------------------------------------------------------------------------

// Protocol adapter operations
typedef struct {
    // Create protocol instance
    ppdb_error_t (*create)(void** proto, void* user_data);
    
    // Destroy protocol instance
    void (*destroy)(void* proto);
    
    // Handle connection established
    ppdb_error_t (*on_connect)(void* proto, ppdb_conn_t conn);
    
    // Handle connection closed
    void (*on_disconnect)(void* proto, ppdb_conn_t conn);
    
    // Handle incoming data
    ppdb_error_t (*on_data)(void* proto, ppdb_conn_t conn,
                           const uint8_t* data, size_t size);
    
    // Get protocol name
    const char* (*get_name)(void* proto);
} peer_ops_t;

//-----------------------------------------------------------------------------
// Connection management
//-----------------------------------------------------------------------------

// Create connection
ppdb_error_t ppdb_conn_create(ppdb_conn_t* conn, const peer_ops_t* ops, void* user_data);

// Destroy connection
void ppdb_conn_destroy(ppdb_conn_t conn);

// Set connection socket
ppdb_error_t ppdb_conn_set_socket(ppdb_conn_t conn, int fd);

// Close connection
void ppdb_conn_close(ppdb_conn_t conn);

// Send data
ppdb_error_t ppdb_conn_send(ppdb_conn_t conn, const void* data, size_t size);

// Receive data
ppdb_error_t ppdb_conn_recv(ppdb_conn_t conn, void* data, size_t size);

// Get connection state
bool ppdb_conn_is_connected(ppdb_conn_t conn);

// Get protocol name
const char* ppdb_conn_get_proto_name(ppdb_conn_t conn);

//-----------------------------------------------------------------------------
// Protocol adapter getters
//-----------------------------------------------------------------------------

// Get memcached protocol adapter
const peer_ops_t* peer_get_memcached(void);

// Get redis protocol adapter
const peer_ops_t* peer_get_redis(void);

//-----------------------------------------------------------------------------
// Core Functions
//-----------------------------------------------------------------------------

// Initialize peer subsystem
int peer_init(void);

// Cleanup peer subsystem
void peer_cleanup(void);

// Check if peer subsystem is initialized
int peer_is_initialized(void);

#endif // PPDB_PEER_H 