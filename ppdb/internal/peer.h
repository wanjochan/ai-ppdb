#ifndef PPDB_INTERNAL_PEER_H
#define PPDB_INTERNAL_PEER_H

#include "core.h"

//-----------------------------------------------------------------------------
// Peer Types
//-----------------------------------------------------------------------------
typedef struct ppdb_peer_connection {
    ppdb_core_async_handle_t* handle;
    ppdb_core_mutex_t* mutex;
    bool connected;
    uint32_t retry_count;
} ppdb_peer_connection_t;

typedef struct ppdb_peer_internal {
    ppdb_peer_config_t config;
    ppdb_core_async_loop_t* loop;
    ppdb_peer_connection_t* conn;
} ppdb_peer_internal_t;

//-----------------------------------------------------------------------------
// Message Types
//-----------------------------------------------------------------------------
typedef enum ppdb_peer_msg_type {
    PPDB_PEER_MSG_HANDSHAKE = 1,
    PPDB_PEER_MSG_DATA = 2,
    PPDB_PEER_MSG_ACK = 3,
    PPDB_PEER_MSG_ERROR = 4
} ppdb_peer_msg_type_t;

typedef struct ppdb_peer_msg_header {
    uint32_t magic;
    uint32_t version;
    ppdb_peer_msg_type_t type;
    uint32_t payload_size;
} ppdb_peer_msg_header_t;

//-----------------------------------------------------------------------------
// Internal Functions
//-----------------------------------------------------------------------------
// Connection Management
ppdb_error_t ppdb_peer_connection_create(ppdb_core_async_loop_t* loop, ppdb_peer_connection_t** conn);
void ppdb_peer_connection_destroy(ppdb_peer_connection_t* conn);
ppdb_error_t ppdb_peer_connection_connect(ppdb_peer_connection_t* conn, const char* host, uint16_t port);
ppdb_error_t ppdb_peer_connection_disconnect(ppdb_peer_connection_t* conn);

// Message Handling
ppdb_error_t ppdb_peer_msg_send(ppdb_peer_connection_t* conn, ppdb_peer_msg_type_t type, const void* payload, size_t size);
ppdb_error_t ppdb_peer_msg_recv(ppdb_peer_connection_t* conn, ppdb_peer_msg_header_t* header, void* payload, size_t* size);

// Protocol Helpers
ppdb_error_t ppdb_peer_handshake(ppdb_peer_connection_t* conn);
ppdb_error_t ppdb_peer_send_ack(ppdb_peer_connection_t* conn);
ppdb_error_t ppdb_peer_send_error(ppdb_peer_connection_t* conn, ppdb_error_t error);

#endif // PPDB_INTERNAL_PEER_H
