#include "InfraxNet.h"
#include "InfraxCore.h"
#include "InfraxMemory.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
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
static void socket_free(InfraxSocket* self);

// Forward declarations
static InfraxMemory* get_memory_manager(void);

// Socket option mapping
static int map_socket_level(int level) {
    switch (level) {
        case INFRAX_SOL_SOCKET:
            return SOL_SOCKET;
        default:
            return level;
    }
}

static int map_socket_option(int option) {
    switch (option) {
        case INFRAX_SO_REUSEADDR:
            return SO_REUSEADDR;
        case INFRAX_SO_KEEPALIVE:
            return SO_KEEPALIVE;
        case INFRAX_SO_RCVTIMEO:
            return SO_RCVTIMEO;
        case INFRAX_SO_SNDTIMEO:
            return SO_SNDTIMEO;
        default:
            return option;
    }
}

// Memory manager instance
static InfraxMemory* get_memory_manager(void) {
    static InfraxMemory* memory = NULL;
    if (!memory) {
        InfraxMemoryConfig config = {
            .initial_size = 1024 * 1024,  // 1MB
            .use_gc = false,
            .use_pool = true,
            .gc_threshold = 0
        };
        memory = InfraxMemoryClass.new(&config);
    }
    return memory;
}

// Instance methods implementations
static InfraxError socket_bind(InfraxSocket* self, const InfraxNetAddr* addr) {
    if (!self || !addr) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket or address");

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(addr->port);
    
    // 直接使用inet_pton，因为在infrax_net_addr_from_string中已经验证过了
    inet_pton(AF_INET, addr->ip, &bind_addr.sin_addr);

    if (bind(self->native_handle, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Bind failed: %s (errno=%d)", strerror(errno), errno);
        return make_error(INFRAX_ERROR_NET_BIND_FAILED_CODE, err_msg);
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
        .recv_timeout_ms = self->config.recv_timeout_ms,
        .reuse_addr = self->config.reuse_addr
    };

    // Create new socket instance
    InfraxSocket* new_socket = socket_new(&config);
    if (!new_socket) {
        close(client_fd);
        return INFRAX_ERROR_NET_SOCKET_FAILED;
    }

    // Close the original socket created by socket_new
    if (new_socket->native_handle >= 0) {
        close(new_socket->native_handle);
    }
    
    // Set client file descriptor
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

static InfraxError socket_sendto(InfraxSocket* self, const void* data, size_t size, size_t* sent, const InfraxNetAddr* addr) {
    if (!self || !data || !sent || !addr) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid arguments");

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(addr->port);
    
    if (inet_pton(AF_INET, addr->ip, &dest_addr.sin_addr) <= 0) {
        return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid IP address format");
    }

    ssize_t result = sendto(self->native_handle, data, size, 0, 
                          (struct sockaddr*)&dest_addr, sizeof(dest_addr));

    if (result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return INFRAX_ERROR_NET_WOULD_BLOCK;
        }
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Send failed: %s (errno=%d)", strerror(errno), errno);
        return make_error(INFRAX_ERROR_NET_SEND_FAILED_CODE, err_msg);
    }

    *sent = result;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError socket_recvfrom(InfraxSocket* self, void* buffer, size_t size, size_t* received, InfraxNetAddr* addr) {
    if (!self || !buffer || !received || !addr) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid arguments");

    struct sockaddr_in src_addr;
    memset(&src_addr, 0, sizeof(src_addr));
    socklen_t addr_len = sizeof(src_addr);

    ssize_t result = recvfrom(self->native_handle, buffer, size, 0, 
                             (struct sockaddr*)&src_addr, &addr_len);

    if (result < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return INFRAX_ERROR_NET_WOULD_BLOCK;
        }
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Receive failed: %s (errno=%d)", strerror(errno), errno);
        return make_error(INFRAX_ERROR_NET_RECV_FAILED_CODE, err_msg);
    }

    // 转换源地址
    char ip[64];
    if (inet_ntop(AF_INET, &src_addr.sin_addr, ip, sizeof(ip)) == NULL) {
        return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Failed to convert source IP address");
    }

    strncpy(addr->ip, ip, sizeof(addr->ip) - 1);
    addr->ip[sizeof(addr->ip) - 1] = '\0';
    addr->port = ntohs(src_addr.sin_port);

    *received = result;
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
    
    // Create socket
    int domain = AF_INET;
    int type = config->is_udp ? SOCK_DGRAM : SOCK_STREAM;
    int protocol = config->is_udp ? IPPROTO_UDP : IPPROTO_TCP;
    
    intptr_t fd = socket(domain, type, protocol);
    if (fd < 0) {
        return NULL;
    }
    
    // Create socket instance
    InfraxSocket* self = get_memory_manager()->alloc(get_memory_manager(), sizeof(InfraxSocket));
    if (!self) {
        close(fd);
        return NULL;
    }
    
    // Initialize socket
    self->config = *config;
    self->native_handle = fd;
    self->is_connected = false;
    
    // Set instance methods
    self->bind = socket_bind;
    self->listen = socket_listen;
    self->accept = socket_accept;
    self->connect = socket_connect;
    self->send = socket_send;
    self->recv = socket_recv;
    self->sendto = socket_sendto;
    self->recvfrom = socket_recvfrom;
    self->set_option = socket_set_option;
    self->get_option = socket_get_option;
    self->set_nonblock = socket_set_nonblock;
    self->set_timeout = socket_set_timeout;
    self->get_local_addr = socket_get_local_addr;
    self->get_peer_addr = socket_get_peer_addr;
    
    // Set socket options
    if (config->reuse_addr) {
        int reuse = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            socket_free(self);
            return NULL;
        }
    }
    
    // Set non-blocking mode
    if (config->is_nonblocking) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            socket_free(self);
            return NULL;
        }
    }
    
    // Set timeouts
    if (config->send_timeout_ms > 0 || config->recv_timeout_ms > 0) {
        struct timeval send_timeout = {
            .tv_sec = config->send_timeout_ms / 1000,
            .tv_usec = (config->send_timeout_ms % 1000) * 1000
        };
        struct timeval recv_timeout = {
            .tv_sec = config->recv_timeout_ms / 1000,
            .tv_usec = (config->recv_timeout_ms % 1000) * 1000
        };
        
        if (config->send_timeout_ms > 0) {
            if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout)) < 0) {
                socket_free(self);
                return NULL;
            }
        }
        
        if (config->recv_timeout_ms > 0) {
            if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout)) < 0) {
                socket_free(self);
                return NULL;
            }
        }
    }
    
    return self;
}

static void socket_free(InfraxSocket* self) {
    if (!self) return;
    
    if (self->native_handle >= 0) {
        close(self->native_handle);
        self->native_handle = -1;
    }
    
    get_memory_manager()->dealloc(get_memory_manager(), self);
}

// Socket class instance
InfraxSocketClassType InfraxSocketClass = {
    .new = socket_new,
    .free = socket_free
};

// Private helper functions implementations
static InfraxError set_socket_option(intptr_t handle, int level, int option, const void* value, size_t len) {
    int sys_level = map_socket_level(level);
    int sys_option = map_socket_option(option);
    
    if (setsockopt(handle, sys_level, sys_option, value, len) < 0) {
        return INFRAX_ERROR_NET_OPTION_FAILED;
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError get_socket_option(intptr_t handle, int level, int option, void* value, size_t* len) {
    int sys_level = map_socket_level(level);
    int sys_option = map_socket_option(option);
    
    socklen_t optlen = *len;
    if (getsockopt(handle, sys_level, sys_option, value, &optlen) < 0) {
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
