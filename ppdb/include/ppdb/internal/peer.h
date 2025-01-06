#ifndef PPDB_INTERNAL_PEER_H_
#define PPDB_INTERNAL_PEER_H_

#include <cosmopolitan.h>
#include <ppdb/ppdb.h>
#include <ppdb/internal/base.h>
#include <ppdb/internal/engine.h>

//-----------------------------------------------------------------------------
// Peer Layer Types
//-----------------------------------------------------------------------------

// Forward declarations
typedef struct ppdb_peer_s ppdb_peer_t;
typedef struct ppdb_peer_connection_s ppdb_peer_connection_t;
typedef struct ppdb_peer_request_s ppdb_peer_request_t;
typedef struct ppdb_peer_response_s ppdb_peer_response_t;

// Peer configuration
typedef struct ppdb_peer_config_s {
    const char* host;           // Host address
    uint16_t port;             // Port number
    uint32_t timeout_ms;       // Operation timeout
    uint32_t max_connections;  // Maximum connections
    uint32_t io_threads;       // Number of IO threads
    bool use_tcp_nodelay;      // TCP_NODELAY option
    bool is_server;            // Server or client mode
} ppdb_peer_config_t;

// Request types
typedef enum ppdb_peer_request_type_e {
    PPDB_PEER_REQ_NONE = 0,
    PPDB_PEER_REQ_GET,
    PPDB_PEER_REQ_SET,
    PPDB_PEER_REQ_DELETE,
    PPDB_PEER_REQ_STATS
} ppdb_peer_request_type_t;

// Request structure
struct ppdb_peer_request_s {
    ppdb_peer_request_type_t type;  // Request type
    ppdb_data_t key;               // Key data
    ppdb_data_t value;             // Value data (for SET)
    uint32_t flags;                // Operation flags
    uint64_t cas;                  // CAS value
};

// Response structure
struct ppdb_peer_response_s {
    ppdb_error_t error;           // Error code
    ppdb_data_t value;           // Response data
    uint32_t flags;              // Response flags
    uint64_t cas;                // New CAS value
};

// Response callback
typedef void (*ppdb_peer_response_callback)(ppdb_peer_connection_t* conn,
                                          const ppdb_peer_response_t* resp,
                                          void* user_data);

// Connection callback
typedef void (*ppdb_peer_connection_callback)(ppdb_peer_connection_t* conn,
                                            ppdb_error_t error,
                                            void* user_data);

//-----------------------------------------------------------------------------
// Peer Layer Interface
//-----------------------------------------------------------------------------

// Peer management
ppdb_error_t ppdb_peer_create(ppdb_peer_t** peer, 
                             const ppdb_peer_config_t* config,
                             ppdb_engine_t* engine);
ppdb_error_t ppdb_peer_destroy(ppdb_peer_t* peer);
ppdb_error_t ppdb_peer_start(ppdb_peer_t* peer);
ppdb_error_t ppdb_peer_stop(ppdb_peer_t* peer);

// Connection management
ppdb_error_t ppdb_peer_connect(ppdb_peer_t* peer,
                              const char* host,
                              uint16_t port,
                              ppdb_peer_connection_t** conn);
ppdb_error_t ppdb_peer_disconnect(ppdb_peer_connection_t* conn);
ppdb_error_t ppdb_peer_set_connection_callback(ppdb_peer_t* peer,
                                             ppdb_peer_connection_callback cb,
                                             void* user_data);

// Request handling
ppdb_error_t ppdb_peer_async_request(ppdb_peer_connection_t* conn,
                                    const ppdb_peer_request_t* req,
                                    ppdb_peer_response_callback cb,
                                    void* user_data);

// Statistics
ppdb_error_t ppdb_peer_get_stats(ppdb_peer_t* peer,
                                char* buffer,
                                size_t size);

#endif // PPDB_INTERNAL_PEER_H_