#ifndef PPDB_PEER_H
#define PPDB_PEER_H

#include <cosmopolitan.h>
#include <ppdb/ppdb.h>

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

typedef struct ppdb_peer_s ppdb_peer_t;
typedef struct ppdb_peer_connection_s ppdb_peer_connection_t;

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
// Callbacks
//-----------------------------------------------------------------------------

typedef void (*ppdb_peer_connection_callback)(
    ppdb_peer_connection_t* conn,
    ppdb_error_t error,
    void* user_data
);

typedef void (*ppdb_peer_request_callback)(
    ppdb_peer_connection_t* conn,
    const ppdb_peer_response_t* resp,
    void* user_data
);

//-----------------------------------------------------------------------------
// Core Functions
//-----------------------------------------------------------------------------

// Create a new peer instance
ppdb_error_t ppdb_peer_create(
    ppdb_peer_t** peer,
    const ppdb_peer_config_t* config,
    ppdb_engine_t* engine
);

// Destroy a peer instance
void ppdb_peer_destroy(ppdb_peer_t* peer);

// Start the peer instance
ppdb_error_t ppdb_peer_start(ppdb_peer_t* peer);

// Stop the peer instance
ppdb_error_t ppdb_peer_stop(ppdb_peer_t* peer);

//-----------------------------------------------------------------------------
// Connection Management
//-----------------------------------------------------------------------------

// Set connection callback
ppdb_error_t ppdb_peer_set_connection_callback(
    ppdb_peer_t* peer,
    ppdb_peer_connection_callback cb,
    void* user_data
);

// Connect to remote peer (client mode)
ppdb_error_t ppdb_peer_connect(
    ppdb_peer_t* peer,
    const char* host,
    uint16_t port,
    ppdb_peer_connection_t** conn
);

// Disconnect from remote peer
ppdb_error_t ppdb_peer_disconnect(ppdb_peer_connection_t* conn);

//-----------------------------------------------------------------------------
// Request/Response
//-----------------------------------------------------------------------------

// Send async request
ppdb_error_t ppdb_peer_async_request(
    ppdb_peer_connection_t* conn,
    const ppdb_peer_request_t* req,
    ppdb_peer_request_callback cb,
    void* user_data
);

// Get peer statistics
ppdb_error_t ppdb_peer_get_stats(
    ppdb_peer_t* peer,
    char* buffer,
    size_t size
);

#endif // PPDB_PEER_H 