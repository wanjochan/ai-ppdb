/**
 * @file InfraxNet.h
 * @brief Network operations functionality for the infrax subsystem
 */

//design pattern: factory

#ifndef PPDB_INFRAX_NET_H
#define PPDB_INFRAX_NET_H

#include <stdbool.h>
#include "internal/infrax/InfraxCore.h"
// #include "cosmopolitan.h"

// Forward declarations
typedef struct InfraxSocket InfraxSocket;
typedef struct InfraxSocketClassType InfraxSocketClassType;

// Network address structure
typedef struct {
    char ip[64];
    uint16_t port;
} InfraxNetAddr;

// Socket configuration
typedef struct {
    bool is_udp;           // true for UDP, false for TCP
    bool is_nonblocking;   // true for non-blocking mode
    bool reuse_addr;       // true to enable SO_REUSEADDR
    uint32_t send_timeout_ms;
    uint32_t recv_timeout_ms;
} InfraxSocketConfig;

// The "static" interface (like static methods in OOP)
struct InfraxSocketClassType {
    InfraxSocket* (*new)(const InfraxSocketConfig* config);
    void (*free)(InfraxSocket* self);
};

// The instance structure
struct InfraxSocket {
    
    // Socket data
    InfraxSocketConfig config;
    intptr_t native_handle;
    bool is_connected;
    InfraxNetAddr local_addr;
    InfraxNetAddr peer_addr;

    // Instance methods
    InfraxError (*bind)(InfraxSocket* self, const InfraxNetAddr* addr);
    InfraxError (*listen)(InfraxSocket* self, int backlog);
    InfraxError (*accept)(InfraxSocket* self, InfraxSocket** client_socket, InfraxNetAddr* client_addr);
    InfraxError (*connect)(InfraxSocket* self, const InfraxNetAddr* addr);
    InfraxError (*send)(InfraxSocket* self, const void* data, size_t size, size_t* sent);
    InfraxError (*recv)(InfraxSocket* self, void* buffer, size_t size, size_t* received);
    InfraxError (*sendto)(InfraxSocket* self, const void* data, size_t size, size_t* sent, const InfraxNetAddr* addr);
    InfraxError (*recvfrom)(InfraxSocket* self, void* buffer, size_t size, size_t* received, InfraxNetAddr* addr);
    InfraxError (*set_option)(InfraxSocket* self, int level, int option, const void* value, size_t len);
    InfraxError (*get_option)(InfraxSocket* self, int level, int option, void* value, size_t* len);
    InfraxError (*set_nonblock)(InfraxSocket* self, bool nonblock);
    InfraxError (*set_timeout)(InfraxSocket* self, uint32_t send_timeout_ms, uint32_t recv_timeout_ms);
    InfraxError (*get_local_addr)(InfraxSocket* self, InfraxNetAddr* addr);
    InfraxError (*get_peer_addr)(InfraxSocket* self, InfraxNetAddr* addr);
};

// The "static" interface instance
extern const InfraxSocketClassType InfraxSocketClass;

// Socket level options (compatible with system socket options)
#define INFRAX_SOL_SOCKET      1
#define INFRAX_SO_REUSEADDR    2
#define INFRAX_SO_KEEPALIVE    3
#define INFRAX_SO_RCVTIMEO     4
#define INFRAX_SO_SNDTIMEO     5

//-----------------------------------------------------------------------------
// Error codes
//-----------------------------------------------------------------------------

// Error codes
#define INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE -100
#define INFRAX_ERROR_NET_SOCKET_FAILED_CODE -101
#define INFRAX_ERROR_NET_BIND_FAILED_CODE -102
#define INFRAX_ERROR_NET_LISTEN_FAILED_CODE -103
#define INFRAX_ERROR_NET_ACCEPT_FAILED_CODE -104
#define INFRAX_ERROR_NET_CONNECT_FAILED_CODE -105
#define INFRAX_ERROR_NET_SEND_FAILED_CODE -106
#define INFRAX_ERROR_NET_RECV_FAILED_CODE -107
#define INFRAX_ERROR_NET_OPTION_FAILED_CODE -108
#define INFRAX_ERROR_NET_ALREADY_CONNECTED_CODE -109
#define INFRAX_ERROR_NET_NOT_CONNECTED_CODE -110
#define INFRAX_ERROR_NET_WOULD_BLOCK_CODE -111

// Error structs
#define INFRAX_ERROR_NET_INVALID_ARGUMENT make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid argument")
#define INFRAX_ERROR_NET_SOCKET_FAILED make_error(INFRAX_ERROR_NET_SOCKET_FAILED_CODE, "Failed to create socket")
#define INFRAX_ERROR_NET_BIND_FAILED make_error(INFRAX_ERROR_NET_BIND_FAILED_CODE, "Failed to bind socket")
#define INFRAX_ERROR_NET_LISTEN_FAILED make_error(INFRAX_ERROR_NET_LISTEN_FAILED_CODE, "Failed to listen on socket")
#define INFRAX_ERROR_NET_ACCEPT_FAILED make_error(INFRAX_ERROR_NET_ACCEPT_FAILED_CODE, "Failed to accept connection")
#define INFRAX_ERROR_NET_CONNECT_FAILED make_error(INFRAX_ERROR_NET_CONNECT_FAILED_CODE, "Failed to connect")
#define INFRAX_ERROR_NET_SEND_FAILED make_error(INFRAX_ERROR_NET_SEND_FAILED_CODE, "Failed to send data")
#define INFRAX_ERROR_NET_RECV_FAILED make_error(INFRAX_ERROR_NET_RECV_FAILED_CODE, "Failed to receive data")
#define INFRAX_ERROR_NET_OPTION_FAILED make_error(INFRAX_ERROR_NET_OPTION_FAILED_CODE, "Failed to set/get socket option")
#define INFRAX_ERROR_NET_ALREADY_CONNECTED make_error(INFRAX_ERROR_NET_ALREADY_CONNECTED_CODE, "Socket is already connected")
#define INFRAX_ERROR_NET_NOT_CONNECTED make_error(INFRAX_ERROR_NET_NOT_CONNECTED_CODE, "Socket is not connected")
#define INFRAX_ERROR_NET_WOULD_BLOCK make_error(INFRAX_ERROR_NET_WOULD_BLOCK_CODE, "Operation would block")

// Utility functions for address conversion
InfraxError infrax_net_addr_from_string(const char* ip, uint16_t port, InfraxNetAddr* addr);
InfraxError infrax_net_addr_to_string(const InfraxNetAddr* addr, char* buffer, size_t size);

#endif /* PPDB_INFRAX_NET_H */
