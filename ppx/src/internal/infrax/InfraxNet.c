#include "InfraxNet.h"
#include "InfraxCore.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

// Private functions declarations
static InfraxError set_socket_option(intptr_t handle, int level, int option, const void* value, size_t len);
static InfraxError get_socket_option(intptr_t handle, int level, int option, void* value, size_t* len);
static InfraxError set_socket_nonblocking(intptr_t handle, bool nonblock);
static InfraxSocket* socket_new(const InfraxSocketConfig* config);

// Instance methods implementations
static InfraxError socket_bind(InfraxSocket* self, const InfraxNetAddr* addr) {
    if (!self || !addr) return INFRAX_ERROR_NET_INVALID_ARGUMENT;

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(addr->port);
    
    if (inet_pton(AF_INET, addr->ip, &bind_addr.sin_addr) <= 0) {
        return INFRAX_ERROR_NET_INVALID_ARGUMENT;
    }

    if (bind(self->native_handle, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
        return INFRAX_ERROR_NET_BIND_FAILED;
    }

    self->local_addr = *addr;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError socket_listen(InfraxSocket* self, int backlog) {
    if (!self) return INFRAX_ERROR_NET_INVALID_ARGUMENT;
    if (self->config.is_udp) return INFRAX_ERROR_NET_INVALID_ARGUMENT;

    if (listen(self->native_handle, backlog) < 0) {
        return INFRAX_ERROR_NET_LISTEN_FAILED;
    }

    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError socket_accept(InfraxSocket* self, InfraxSocket** client_socket, InfraxNetAddr* client_addr) {
    if (!self || !client_socket) return INFRAX_ERROR_NET_INVALID_ARGUMENT;
    if (self->config.is_udp) return INFRAX_ERROR_NET_INVALID_ARGUMENT;

    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    intptr_t client_fd = accept(self->native_handle, (struct sockaddr*)&addr, &addr_len);
    
    if (client_fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return INFRAX_ERROR_NET_WOULD_BLOCK;
        }
        return INFRAX_ERROR_NET_ACCEPT_FAILED;
    }

    // Create new socket configuration for client
    InfraxSocketConfig config = {
        .is_udp = false,
        .is_nonblocking = self->config.is_nonblocking,
        .send_timeout_ms = self->config.send_timeout_ms,
        .recv_timeout_ms = self->config.recv_timeout_ms
    };

    // Create new socket instance
    InfraxSocket* new_socket = socket_new(&config);
    if (!new_socket) {
        close(client_fd);
        return INFRAX_ERROR_NET_SOCKET_FAILED;
    }

    // Set client file descriptor
    close(new_socket->native_handle);  // Close the original socket
    new_socket->native_handle = client_fd;
    new_socket->is_connected = true;

    // Set client address if requested
    if (client_addr) {
        client_addr->port = ntohs(addr.sin_port);
        inet_ntop(AF_INET, &addr.sin_addr, client_addr->ip, sizeof(client_addr->ip));
        new_socket->peer_addr = *client_addr;
    }

    *client_socket = new_socket;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError socket_connect(InfraxSocket* self, const InfraxNetAddr* addr) {
    if (!self || !addr) return INFRAX_ERROR_NET_INVALID_ARGUMENT;
    if (self->is_connected) return INFRAX_ERROR_NET_INVALID_ARGUMENT;

    struct sockaddr_in connect_addr;
    memset(&connect_addr, 0, sizeof(connect_addr));
    connect_addr.sin_family = AF_INET;
    connect_addr.sin_port = htons(addr->port);
    
    if (inet_pton(AF_INET, addr->ip, &connect_addr.sin_addr) <= 0) {
        return INFRAX_ERROR_NET_INVALID_ARGUMENT;
    }

    if (connect(self->native_handle, (struct sockaddr*)&connect_addr, sizeof(connect_addr)) < 0) {
        if (errno != EINPROGRESS) {
            return INFRAX_ERROR_NET_CONNECT_FAILED;
        }
    }

    self->peer_addr = *addr;
    self->is_connected = true;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError socket_send(InfraxSocket* self, const void* data, size_t size, size_t* sent) {
    if (!self || !data || !sent) return INFRAX_ERROR_NET_INVALID_ARGUMENT;
    if (!self->is_connected && !self->config.is_udp) return INFRAX_ERROR_NET_NOT_CONNECTED;

    ssize_t result;
    if (self->config.is_udp) {
        // For UDP, we need to send to the peer address
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(self->peer_addr.port);
        inet_pton(AF_INET, self->peer_addr.ip, &addr.sin_addr);
        result = sendto(self->native_handle, data, size, 0, (struct sockaddr*)&addr, sizeof(addr));
    } else {
        result = send(self->native_handle, data, size, 0);
    }

    if (result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *sent = 0;
            return INFRAX_ERROR_NET_WOULD_BLOCK;
        }
        *sent = 0;
        return INFRAX_ERROR_NET_SEND_FAILED;
    }

    *sent = (size_t)result;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError socket_recv(InfraxSocket* self, void* buffer, size_t size, size_t* received) {
    if (!self || !buffer || !received) return INFRAX_ERROR_NET_INVALID_ARGUMENT;
    if (!self->is_connected && !self->config.is_udp) return INFRAX_ERROR_NET_NOT_CONNECTED;

    ssize_t result;
    if (self->config.is_udp) {
        // For UDP, we need to receive from any address
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        result = recvfrom(self->native_handle, buffer, size, 0, (struct sockaddr*)&addr, &addr_len);
        if (result >= 0) {
            // Store the peer address for future sends
            self->peer_addr.port = ntohs(addr.sin_port);
            inet_ntop(AF_INET, &addr.sin_addr, self->peer_addr.ip, sizeof(self->peer_addr.ip));
        }
    } else {
        result = recv(self->native_handle, buffer, size, 0);
    }

    if (result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *received = 0;
            return INFRAX_ERROR_NET_WOULD_BLOCK;
        }
        *received = 0;
        return INFRAX_ERROR_NET_RECV_FAILED;
    }

    *received = (size_t)result;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError socket_set_option(InfraxSocket* self, int level, int option, const void* value, size_t len) {
    if (!self || !value) return INFRAX_ERROR_NET_INVALID_ARGUMENT;
    return set_socket_option(self->native_handle, level, option, value, len);
}

static InfraxError socket_get_option(InfraxSocket* self, int level, int option, void* value, size_t* len) {
    if (!self || !value || !len) return INFRAX_ERROR_NET_INVALID_ARGUMENT;
    return get_socket_option(self->native_handle, level, option, value, len);
}

static InfraxError socket_set_nonblock(InfraxSocket* self, bool nonblock) {
    if (!self) return INFRAX_ERROR_NET_INVALID_ARGUMENT;
    
    InfraxError err = set_socket_nonblocking(self->native_handle, nonblock);
    if (INFRAX_ERROR_IS_OK(err)) {
        self->config.is_nonblocking = nonblock;
    }
    return err;
}

static InfraxError socket_set_timeout(InfraxSocket* self, uint32_t send_timeout_ms, uint32_t recv_timeout_ms) {
    if (!self) return INFRAX_ERROR_NET_INVALID_ARGUMENT;

    struct timeval send_tv = {
        .tv_sec = send_timeout_ms / 1000,
        .tv_usec = (send_timeout_ms % 1000) * 1000
    };

    struct timeval recv_tv = {
        .tv_sec = recv_timeout_ms / 1000,
        .tv_usec = (recv_timeout_ms % 1000) * 1000
    };

    InfraxError err = set_socket_option(self->native_handle, SOL_SOCKET, SO_SNDTIMEO, &send_tv, sizeof(send_tv));
    if (INFRAX_ERROR_IS_ERR(err)) return err;

    err = set_socket_option(self->native_handle, SOL_SOCKET, SO_RCVTIMEO, &recv_tv, sizeof(recv_tv));
    if (INFRAX_ERROR_IS_ERR(err)) return err;

    self->config.send_timeout_ms = send_timeout_ms;
    self->config.recv_timeout_ms = recv_timeout_ms;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError socket_get_local_addr(InfraxSocket* self, InfraxNetAddr* addr) {
    if (!self || !addr) return INFRAX_ERROR_NET_INVALID_ARGUMENT;
    
    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    
    if (getsockname(self->native_handle, (struct sockaddr*)&local_addr, &addr_len) < 0) {
        return INFRAX_ERROR_NET_OPTION_FAILED;
    }

    addr->port = ntohs(local_addr.sin_port);
    inet_ntop(AF_INET, &local_addr.sin_addr, addr->ip, sizeof(addr->ip));
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError socket_get_peer_addr(InfraxSocket* self, InfraxNetAddr* addr) {
    if (!self || !addr) return INFRAX_ERROR_NET_INVALID_ARGUMENT;
    if (!self->is_connected) return INFRAX_ERROR_NET_INVALID_ARGUMENT;
    
    struct sockaddr_in peer_addr;
    socklen_t addr_len = sizeof(peer_addr);
    
    if (getpeername(self->native_handle, (struct sockaddr*)&peer_addr, &addr_len) < 0) {
        return INFRAX_ERROR_NET_OPTION_FAILED;
    }

    addr->port = ntohs(peer_addr.sin_port);
    inet_ntop(AF_INET, &peer_addr.sin_addr, addr->ip, sizeof(addr->ip));
    return INFRAX_ERROR_OK_STRUCT;
}

// Constructor and destructor
static InfraxSocket* socket_new(const InfraxSocketConfig* config) {
    if (!config) return NULL;

    // Allocate memory for the socket instance
    InfraxSocket* self = (InfraxSocket*)malloc(sizeof(InfraxSocket));
    if (!self) return NULL;

    // Initialize the socket
    memset(self, 0, sizeof(InfraxSocket));
    self->klass = &InfraxSocket_CLASS;
    self->config = *config;

    // Create the native socket
    int type = config->is_udp ? SOCK_DGRAM : SOCK_STREAM;
    int sock = socket(AF_INET, type, 0);
    if (sock < 0) {
        free(self);
        return NULL;
    }
    self->native_handle = sock;

    // Set initial socket options
    if (config->is_nonblocking) {
        InfraxError err = set_socket_nonblocking(sock, true);
        if (!INFRAX_ERROR_IS_OK(err)) {
            close(sock);
            free(self);
            return NULL;
        }
    }

    if (config->send_timeout_ms > 0 || config->recv_timeout_ms > 0) {
        InfraxError err = socket_set_timeout(self, config->send_timeout_ms, config->recv_timeout_ms);
        if (!INFRAX_ERROR_IS_OK(err)) {
            close(sock);
            free(self);
            return NULL;
        }
    }

    // Set SO_REUSEADDR option
    int reuse = 1;
    InfraxError err = set_socket_option(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (!INFRAX_ERROR_IS_OK(err)) {
        close(sock);
        free(self);
        return NULL;
    }

    // Set SO_REUSEPORT option
    err = set_socket_option(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
    if (!INFRAX_ERROR_IS_OK(err)) {
        close(sock);
        free(self);
        return NULL;
    }

    // Initialize instance methods
    self->bind = socket_bind;
    self->listen = socket_listen;
    self->accept = socket_accept;
    self->connect = socket_connect;
    self->send = socket_send;
    self->recv = socket_recv;
    self->set_option = socket_set_option;
    self->get_option = socket_get_option;
    self->set_nonblock = socket_set_nonblock;
    self->set_timeout = socket_set_timeout;
    self->get_local_addr = socket_get_local_addr;
    self->get_peer_addr = socket_get_peer_addr;

    return self;
}

static void socket_free(InfraxSocket* self) {
    if (self) {
        if (self->native_handle >= 0) {
            close(self->native_handle);
        }
        free(self);
    }
}

// The "static" interface instance
const InfraxSocketClass InfraxSocket_CLASS = {
    .new = socket_new,
    .free = socket_free
};

// Private helper functions implementations
static InfraxError set_socket_option(intptr_t handle, int level, int option, const void* value, size_t len) {
    if (setsockopt(handle, level, option, value, len) < 0) {
        return INFRAX_ERROR_NET_OPTION_FAILED;
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError get_socket_option(intptr_t handle, int level, int option, void* value, size_t* len) {
    socklen_t optlen = *len;
    if (getsockopt(handle, level, option, value, &optlen) < 0) {
        return INFRAX_ERROR_NET_OPTION_FAILED;
    }
    *len = optlen;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError set_socket_nonblocking(intptr_t handle, bool nonblock) {
    int flags = fcntl(handle, F_GETFL, 0);
    if (flags < 0) return INFRAX_ERROR_NET_OPTION_FAILED;

    flags = nonblock ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (fcntl(handle, F_SETFL, flags) < 0) {
        return INFRAX_ERROR_NET_OPTION_FAILED;
    }

    return INFRAX_ERROR_OK_STRUCT;
}

// Utility functions implementations
InfraxError infrax_net_addr_from_string(const char* ip, uint16_t port, InfraxNetAddr* addr) {
    if (!ip || !addr) return INFRAX_ERROR_NET_INVALID_ARGUMENT;
    
    struct in_addr inaddr;
    if (inet_pton(AF_INET, ip, &inaddr) <= 0) {
        return INFRAX_ERROR_NET_INVALID_ARGUMENT;
    }

    strncpy(addr->ip, ip, sizeof(addr->ip) - 1);
    addr->ip[sizeof(addr->ip) - 1] = '\0';
    addr->port = port;
    return INFRAX_ERROR_OK_STRUCT;
}

InfraxError infrax_net_addr_to_string(const InfraxNetAddr* addr, char* buffer, size_t size) {
    if (!addr || !buffer || size == 0) return INFRAX_ERROR_NET_INVALID_ARGUMENT;
    
    int result = snprintf(buffer, size, "%s:%u", addr->ip, addr->port);
    if (result < 0 || (size_t)result >= size) {
        return INFRAX_ERROR_NET_INVALID_ARGUMENT;
    }
    
    return INFRAX_ERROR_OK_STRUCT;
}
