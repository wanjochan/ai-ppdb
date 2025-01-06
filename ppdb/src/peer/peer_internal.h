#ifndef PPDB_PEER_INTERNAL_H_
#define PPDB_PEER_INTERNAL_H_

#include "internal/peer.h"
#include "internal/base.h"

//-----------------------------------------------------------------------------
// Internal Types
//-----------------------------------------------------------------------------

// Connection states
typedef enum ppdb_peer_conn_state_e {
    PPDB_PEER_CONN_INIT = 0,
    PPDB_PEER_CONN_CONNECTING,
    PPDB_PEER_CONN_CONNECTED,
    PPDB_PEER_CONN_CLOSING,
    PPDB_PEER_CONN_CLOSED
} ppdb_peer_conn_state_t;

// Protocol states
typedef enum ppdb_peer_proto_state_e {
    PPDB_PEER_PROTO_INIT = 0,
    PPDB_PEER_PROTO_HEADER,
    PPDB_PEER_PROTO_KEY,
    PPDB_PEER_PROTO_VALUE,
    PPDB_PEER_PROTO_COMPLETE
} ppdb_peer_proto_state_t;

// Protocol header
typedef struct ppdb_peer_proto_header_s {
    uint8_t magic;          // Protocol magic
    uint8_t opcode;         // Operation code
    uint16_t key_len;       // Key length
    uint8_t extras_len;     // Extras length
    uint8_t data_type;      // Data type
    uint16_t status;        // Status code
    uint32_t body_len;      // Body length
    uint32_t opaque;        // Operation ID
    uint64_t cas;           // CAS value
} ppdb_peer_proto_header_t;

// Connection structure
struct ppdb_peer_connection_s {
    ppdb_peer_t* peer;                    // Owner peer
    ppdb_peer_conn_state_t state;         // Connection state
    ppdb_base_async_handle_t* handle;     // Async handle
    ppdb_peer_proto_state_t proto_state;  // Protocol state
    ppdb_peer_proto_header_t header;      // Current header
    ppdb_peer_request_t current_req;      // Current request
    ppdb_peer_response_t current_resp;    // Current response
    ppdb_peer_response_callback callback; // Response callback
    void* user_data;                     // User data
    struct {
        char* buf;                       // Read buffer
        size_t size;                     // Buffer size
        size_t pos;                      // Current position
    } read;
    struct {
        char* buf;                       // Write buffer
        size_t size;                     // Buffer size
        size_t pos;                      // Current position
    } write;
};

// Peer structure
struct ppdb_peer_s {
    ppdb_peer_config_t config;           // Configuration
    ppdb_engine_t* engine;               // Storage engine
    ppdb_base_async_loop_t* loop;        // Event loop
    ppdb_base_mutex_t* mutex;            // Protect state
    ppdb_peer_connection_callback conn_cb; // Connection callback
    void* user_data;                     // User data
    bool running;                        // Is running
    struct {
        uint64_t total_connections;      // Total connections
        uint64_t active_connections;     // Active connections
        uint64_t total_requests;         // Total requests
        uint64_t failed_requests;        // Failed requests
    } stats;
};

//-----------------------------------------------------------------------------
// Internal Functions
//-----------------------------------------------------------------------------

// Protocol functions
ppdb_error_t ppdb_peer_proto_parse(ppdb_peer_connection_t* conn,
                                  const char* data,
                                  size_t len);
ppdb_error_t ppdb_peer_proto_format(ppdb_peer_connection_t* conn,
                                   const ppdb_peer_response_t* resp,
                                   char* buf,
                                   size_t* len);

// Connection functions
ppdb_error_t ppdb_peer_conn_create(ppdb_peer_t* peer,
                                  ppdb_peer_connection_t** conn);
void ppdb_peer_conn_destroy(ppdb_peer_connection_t* conn);
ppdb_error_t ppdb_peer_conn_start_read(ppdb_peer_connection_t* conn);
ppdb_error_t ppdb_peer_conn_start_write(ppdb_peer_connection_t* conn);

// Async operation functions
ppdb_error_t ppdb_peer_async_handle_request(ppdb_peer_connection_t* conn,
                                          const ppdb_peer_request_t* req);
void ppdb_peer_async_complete(ppdb_peer_connection_t* conn,
                            ppdb_error_t error,
                            const ppdb_peer_response_t* resp);

#endif // PPDB_PEER_INTERNAL_H_