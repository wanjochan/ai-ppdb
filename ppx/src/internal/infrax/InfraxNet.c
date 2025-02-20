#include "InfraxNet.h"
#include "InfraxCore.h"
#include "InfraxMemory.h"

// Private functions declarations
static InfraxError set_socket_option(intptr_t handle, int level, int option, const void* value, size_t len);
static InfraxError get_socket_option(intptr_t handle, int level, int option, void* value, size_t* len);
static InfraxError set_socket_nonblocking(intptr_t handle, InfraxBool nonblocking);
static InfraxNet* net_new(const InfraxNetConfig* config);
static void net_free(InfraxNet* self);
static InfraxError net_shutdown(InfraxNet* self, int how);

// Forward declarations
static InfraxMemory* get_memory_manager(void);

// Socket option mapping
static int map_socket_level(int level) {
    switch (level) {
        case INFRAX_SOL_SOCKET:
            return INFRAX_SOL_SOCKET;
        default:
            return level;
    }
}

static int map_socket_option(int option) {
    switch (option) {
        case INFRAX_SO_REUSEADDR:
            return INFRAX_SO_REUSEADDR;
        case INFRAX_SO_KEEPALIVE:
            return INFRAX_SO_KEEPALIVE;
        case INFRAX_SO_RCVTIMEO:
            return INFRAX_SO_RCVTIMEO;
        case INFRAX_SO_SNDTIMEO:
            return INFRAX_SO_SNDTIMEO;
        case INFRAX_SO_RCVBUF:
            return INFRAX_SO_RCVBUF;
        case INFRAX_SO_SNDBUF:
            return INFRAX_SO_SNDBUF;
        case INFRAX_SO_ERROR:
            return INFRAX_SO_ERROR;
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
            .use_gc = INFRAX_FALSE,
            .use_pool = INFRAX_TRUE,
            .gc_threshold = 0
        };
        memory = InfraxMemoryClass.new(&config);
    }
    return memory;
}

// Instance methods implementations
static InfraxError net_bind(InfraxNet* self, const InfraxNetAddr* addr) {
    if (!self || !addr) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket or address");

    // 验证端口号
    if (addr->port == 0) {
        return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid port number: 0 is not allowed");
    }

    InfraxSockAddrIn bind_addr;
    gInfraxCore.memset(&gInfraxCore, &bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = INFRAX_AF_INET;
    bind_addr.sin_port = gInfraxCore.host_to_net16(&gInfraxCore, addr->port);
    
    // 验证 IP 地址格式
    if (gInfraxCore.ip_to_binary(&gInfraxCore, addr->ip, &bind_addr.sin_addr, sizeof(bind_addr.sin_addr)) <= 0) {
        return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid IP address format");
    }

    if (gInfraxCore.socket_bind(&gInfraxCore, self->native_handle, &bind_addr, sizeof(bind_addr)) < 0) {
        char err_msg[256];
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Bind failed: %s (errno=%d)", 
            gInfraxCore.get_error_string(&gInfraxCore, err), err);
        return make_error(INFRAX_ERROR_NET_BIND_FAILED_CODE, err_msg);
    }

    self->local_addr = *addr;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_listen(InfraxNet* self, int backlog) {
    if (!self) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket");
    if (self->config.is_udp) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "UDP socket cannot listen");

    if (gInfraxCore.socket_listen(&gInfraxCore, self->native_handle, backlog) < 0) {
        char err_msg[256];
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Listen failed: %s (errno=%d)", 
            gInfraxCore.get_error_string(&gInfraxCore, err), err);
        return make_error(INFRAX_ERROR_NET_LISTEN_FAILED_CODE, err_msg);
    }

    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_shutdown(InfraxNet* self, int how) {
    if (!self) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket");
    if (self->native_handle < 0) return INFRAX_ERROR_OK_STRUCT;  // 已经关闭
    
    // 映射 shutdown 模式
    int sys_how;
    switch (how) {
        case INFRAX_SHUT_RD:
            sys_how = INFRAX_SHUT_RD;
            break;
        case INFRAX_SHUT_WR:
            sys_how = INFRAX_SHUT_WR;
            break;
        case INFRAX_SHUT_RDWR:
            sys_how = INFRAX_SHUT_RDWR;
            break;
        default:
            return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid shutdown mode");
    }
    
    if (gInfraxCore.socket_shutdown(&gInfraxCore, self->native_handle, sys_how) < 0) {
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        // 忽略某些特定的错误
        if (err == INFRAX_ENOTCONN) {
            // socket未连接，这是可以接受的
            return INFRAX_ERROR_OK_STRUCT;
        }
        
        char err_msg[256];
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Shutdown failed: %s (errno=%d)", 
            gInfraxCore.get_error_string(&gInfraxCore, err), err);
        return make_error(INFRAX_ERROR_NET_SOCKET_FAILED_CODE, err_msg);
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_close(InfraxNet* self) {
    if (!self) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket");
    if (self->native_handle < 0) return INFRAX_ERROR_OK_STRUCT;  // 已经关闭
    
    // 关闭socket
    if (gInfraxCore.socket_close(&gInfraxCore, self->native_handle) < 0) {
        char err_msg[256];
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Close failed: %s (errno=%d)", 
            gInfraxCore.get_error_string(&gInfraxCore, err), err);
        return make_error(INFRAX_ERROR_NET_SOCKET_FAILED_CODE, err_msg);
    }
    
    self->native_handle = -1;
    self->is_connected = INFRAX_FALSE;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_accept(InfraxNet* self, InfraxNet** client_socket, InfraxNetAddr* client_addr) {
    if (!self || !client_socket) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket or client socket pointer");
    if (self->config.is_udp) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "UDP socket cannot accept");

    InfraxSockAddrIn addr;
    size_t addr_len = sizeof(addr);
    intptr_t client_fd = gInfraxCore.socket_accept(&gInfraxCore, self->native_handle, &addr, &addr_len);
    
    if (client_fd < 0) {
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        if (err == INFRAX_EAGAIN || err == INFRAX_EWOULDBLOCK) {
            return make_error(INFRAX_ERROR_NET_WOULD_BLOCK_CODE, "Accept would block");
        }
        char err_msg[256];
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Accept failed: %s (errno=%d)", 
            gInfraxCore.get_error_string(&gInfraxCore, err), err);
        return make_error(INFRAX_ERROR_NET_ACCEPT_FAILED_CODE, err_msg);
    }

    // Create new socket configuration for client
    InfraxNetConfig config = {
        .is_udp = INFRAX_FALSE,
        .is_nonblocking = self->config.is_nonblocking,
        .send_timeout_ms = self->config.send_timeout_ms,
        .recv_timeout_ms = self->config.recv_timeout_ms,
        .reuse_addr = self->config.reuse_addr
    };

    // Create new socket instance
    InfraxNet* new_socket = net_new(&config);
    if (!new_socket) {
        gInfraxCore.socket_close(&gInfraxCore, client_fd);
        return make_error(INFRAX_ERROR_NET_SOCKET_FAILED_CODE, "Failed to create new socket instance");
    }

    // Close the original socket created by net_new
    if (new_socket->native_handle >= 0) {
        gInfraxCore.socket_close(&gInfraxCore, new_socket->native_handle);
    }
    
    // Set client file descriptor
    new_socket->native_handle = client_fd;
    new_socket->is_connected = INFRAX_TRUE;

    // Set client address if requested
    if (client_addr) {
        client_addr->port = gInfraxCore.net_to_host16(&gInfraxCore, addr.sin_port);
        gInfraxCore.binary_to_ip(&gInfraxCore, &addr.sin_addr, client_addr->ip, sizeof(client_addr->ip));
        new_socket->peer_addr = *client_addr;
    }

    *client_socket = new_socket;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_connect(InfraxNet* self, const InfraxNetAddr* addr) {
    if (!self || !addr) {
        return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid arguments");
    }

    if (self->native_handle < 0) {
        return make_error(INFRAX_ERROR_NET_INVALID_STATE_CODE, "Socket not initialized");
    }

    InfraxSockAddrIn connect_addr;
    gInfraxCore.memset(&gInfraxCore, &connect_addr, 0, sizeof(connect_addr));
    connect_addr.sin_family = INFRAX_AF_INET;
    connect_addr.sin_port = gInfraxCore.host_to_net16(&gInfraxCore, addr->port);
    
    if (gInfraxCore.ip_to_binary(&gInfraxCore, addr->ip, &connect_addr.sin_addr, sizeof(connect_addr.sin_addr)) < 0) {
        return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid IP address");
    }

    if (gInfraxCore.socket_connect(&gInfraxCore, self->native_handle, &connect_addr, sizeof(connect_addr)) < 0) {
        int err = gInfraxCore.get_last_error(&gInfraxCore);
        if (err == INFRAX_EINPROGRESS) {
            // 使用 select 等待连接完成或超时
            InfraxError wait_err = gInfraxCore.wait_for_write(&gInfraxCore, self->native_handle, self->config.send_timeout_ms);
            if (wait_err.code != INFRAX_ERROR_OK) {
                return wait_err;
            }

            // 检查连接是否成功
            int socket_err;
            size_t socket_err_len = sizeof(socket_err);
            if (gInfraxCore.socket_get_option(&gInfraxCore, self->native_handle, INFRAX_SOL_SOCKET, INFRAX_SO_ERROR, &socket_err, &socket_err_len) < 0) {
                return make_error(INFRAX_ERROR_NET_CONNECT_FAILED_CODE, "Failed to get socket error");
            }

            if (socket_err != 0) {
                char err_msg[256];
                gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Connect failed: %s (errno=%d)", 
                    gInfraxCore.get_error_string(&gInfraxCore, socket_err), socket_err);
                return make_error(INFRAX_ERROR_NET_CONNECT_FAILED_CODE, err_msg);
            }
        } else {
            char err_msg[256];
            gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Connect failed: %s (errno=%d)", 
                gInfraxCore.get_error_string(&gInfraxCore, err), err);
            return make_error(INFRAX_ERROR_NET_CONNECT_FAILED_CODE, err_msg);
        }
    }

    self->is_connected = INFRAX_TRUE;
    self->peer_addr = *addr;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_send(InfraxNet* self, const void* data, size_t size, size_t* sent_size) {
    if (!self || !data || !sent_size) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket, data or sent_size pointer");
    if (self->native_handle < 0) return make_error(INFRAX_ERROR_NET_INVALID_OPERATION_CODE, "Socket is not open");
    if (!self->is_connected && !self->config.is_udp) return make_error(INFRAX_ERROR_NET_NOT_CONNECTED_CODE, "Socket is not connected");

    ssize_t result = gInfraxCore.socket_send(&gInfraxCore, self->native_handle, data, size, 0);
    if (result < 0) {
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        if (err == INFRAX_EAGAIN || err == INFRAX_EWOULDBLOCK) {
            *sent_size = 0;
            return make_error(INFRAX_ERROR_NET_WOULD_BLOCK_CODE, "Send would block");
        }
        char err_msg[256];
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Send failed: %s (errno=%d)", 
            gInfraxCore.get_error_string(&gInfraxCore, err), err);
        return make_error(INFRAX_ERROR_NET_SEND_FAILED_CODE, err_msg);
    }

    *sent_size = result;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_recv(InfraxNet* self, void* buffer, size_t size, size_t* received_size) {
    if (!self || !buffer || !received_size) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket, buffer or received_size pointer");
    if (self->native_handle < 0) return make_error(INFRAX_ERROR_NET_INVALID_OPERATION_CODE, "Socket is not open");
    if (!self->is_connected && !self->config.is_udp) return make_error(INFRAX_ERROR_NET_NOT_CONNECTED_CODE, "Socket is not connected");

    ssize_t result = gInfraxCore.socket_recv(&gInfraxCore, self->native_handle, buffer, size, 0);
    if (result < 0) {
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        if (err == INFRAX_EAGAIN || err == INFRAX_EWOULDBLOCK) {
            *received_size = 0;
            return make_error(INFRAX_ERROR_NET_WOULD_BLOCK_CODE, "Receive would block");
        }
        char err_msg[256];
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Receive failed: %s (errno=%d)", 
            gInfraxCore.get_error_string(&gInfraxCore, err), err);
        return make_error(INFRAX_ERROR_NET_RECV_FAILED_CODE, err_msg);
    }

    *received_size = result;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_sendto(InfraxNet* self, const void* data, size_t size, size_t* sent_size, const InfraxNetAddr* addr) {
    if (!self || !data || !addr || !sent_size) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket, data, address or sent_size pointer");
    if (self->native_handle < 0) return make_error(INFRAX_ERROR_NET_INVALID_OPERATION_CODE, "Socket is not open");
    if (!self->config.is_udp) return make_error(INFRAX_ERROR_NET_INVALID_OPERATION_CODE, "Socket is not UDP");

    InfraxSockAddrIn send_addr;
    gInfraxCore.memset(&gInfraxCore, &send_addr, 0, sizeof(send_addr));
    send_addr.sin_family = INFRAX_AF_INET;
    send_addr.sin_port = gInfraxCore.host_to_net16(&gInfraxCore, addr->port);
    
    if (gInfraxCore.ip_to_binary(&gInfraxCore, addr->ip, &send_addr.sin_addr, sizeof(send_addr.sin_addr)) <= 0) {
        return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid IP address format");
    }

    ssize_t result = gInfraxCore.socket_send(&gInfraxCore, self->native_handle, data, size, 0);
    if (result < 0) {
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        if (err == INFRAX_EAGAIN || err == INFRAX_EWOULDBLOCK) {
            *sent_size = 0;
            return make_error(INFRAX_ERROR_NET_WOULD_BLOCK_CODE, "Send would block");
        }
        char err_msg[256];
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Send failed: %s (errno=%d)", 
            gInfraxCore.get_error_string(&gInfraxCore, err), err);
        return make_error(INFRAX_ERROR_NET_SEND_FAILED_CODE, err_msg);
    }

    *sent_size = result;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_recvfrom(InfraxNet* self, void* buffer, size_t size, size_t* received, InfraxNetAddr* addr) {
    if (!self || !buffer || !received) {
        return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid arguments");
    }

    if (self->native_handle < 0) {
        return make_error(INFRAX_ERROR_NET_INVALID_STATE_CODE, "Socket not initialized");
    }

    InfraxSockAddrIn recv_addr;
    size_t addr_len = sizeof(recv_addr);
    
    ssize_t result = gInfraxCore.socket_recv(&gInfraxCore, self->native_handle, buffer, size, 0);
    
    if (result < 0) {
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        if (err == INFRAX_EAGAIN || err == INFRAX_EWOULDBLOCK) {
            return make_error(INFRAX_ERROR_NET_WOULD_BLOCK_CODE, "Receive would block");
        }
        char err_msg[256];
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Receive failed: %s (errno=%d)", 
            gInfraxCore.get_error_string(&gInfraxCore, err), err);
        return make_error(INFRAX_ERROR_NET_RECV_FAILED_CODE, err_msg);
    }

    *received = (size_t)result;

    if (addr) {
        addr->port = gInfraxCore.net_to_host16(&gInfraxCore, recv_addr.sin_port);
        gInfraxCore.binary_to_ip(&gInfraxCore, &recv_addr.sin_addr, addr->ip, sizeof(addr->ip));
    }

    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_set_option(InfraxNet* self, int level, int option, const void* value, size_t len) {
    if (!self || !value) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket or value pointer");
    if (self->native_handle < 0) return make_error(INFRAX_ERROR_NET_INVALID_OPERATION_CODE, "Socket is not open");

    int sys_level = map_socket_level(level);
    int sys_option = map_socket_option(option);

    if (gInfraxCore.socket_set_option(&gInfraxCore, self->native_handle, sys_level, sys_option, value, len) < 0) {
        char err_msg[256];
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Set socket option failed: %s (errno=%d)", 
            gInfraxCore.get_error_string(&gInfraxCore, err), err);
        return make_error(INFRAX_ERROR_NET_SET_OPTION_FAILED_CODE, err_msg);
    }

    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_get_option(InfraxNet* self, int level, int option, void* value, size_t* len) {
    if (!self || !value || !len) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket, value or len pointer");
    if (self->native_handle < 0) return make_error(INFRAX_ERROR_NET_INVALID_OPERATION_CODE, "Socket is not open");

    int sys_level = map_socket_level(level);
    int sys_option = map_socket_option(option);

    if (gInfraxCore.socket_get_option(&gInfraxCore, self->native_handle, sys_level, sys_option, value, len) < 0) {
        char err_msg[256];
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Get socket option failed: %s (errno=%d)", 
            gInfraxCore.get_error_string(&gInfraxCore, err), err);
        return make_error(INFRAX_ERROR_NET_GET_OPTION_FAILED_CODE, err_msg);
    }

    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError set_socket_nonblocking(intptr_t handle, InfraxBool nonblocking) {
    int flags = gInfraxCore.fcntl(&gInfraxCore, handle, INFRAX_F_GETFL, 0);
    if (flags < 0) return INFRAX_ERROR_NET_OPTION_FAILED;

    flags = nonblocking ? (flags | INFRAX_O_NONBLOCK) : (flags & ~INFRAX_O_NONBLOCK);
    if (gInfraxCore.fcntl(&gInfraxCore, handle, INFRAX_F_SETFL, flags) < 0) {
        return INFRAX_ERROR_NET_OPTION_FAILED;
    }

    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_set_nonblocking(InfraxNet* self, InfraxBool nonblocking) {
    if (!self) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket");
    if (self->native_handle < 0) return make_error(INFRAX_ERROR_NET_INVALID_OPERATION_CODE, "Socket is not open");

    InfraxError err = set_socket_nonblocking(self->native_handle, nonblocking);
    if (INFRAX_ERROR_IS_ERR(err)) {
        return err;
    }

    self->config.is_nonblocking = nonblocking;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_set_timeout(InfraxNet* self, uint32_t send_timeout_ms, uint32_t recv_timeout_ms) {
    if (!self) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket");
    if (self->native_handle < 0) return make_error(INFRAX_ERROR_NET_INVALID_OPERATION_CODE, "Socket is not open");

    InfraxTimeVal send_tv = {
        .tv_sec = send_timeout_ms / 1000,
        .tv_usec = (send_timeout_ms % 1000) * 1000
    };

    InfraxTimeVal recv_tv = {
        .tv_sec = recv_timeout_ms / 1000,
        .tv_usec = (recv_timeout_ms % 1000) * 1000
    };

    // 设置发送超时
    if (gInfraxCore.socket_set_option(&gInfraxCore, self->native_handle, INFRAX_SOL_SOCKET, INFRAX_SO_SNDTIMEO, &send_tv, sizeof(send_tv)) < 0) {
        char err_msg[256];
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Set send timeout failed: %s (errno=%d)", 
            gInfraxCore.get_error_string(&gInfraxCore, err), err);
        return make_error(INFRAX_ERROR_NET_SET_TIMEOUT_FAILED_CODE, err_msg);
    }

    // 设置接收超时
    if (gInfraxCore.socket_set_option(&gInfraxCore, self->native_handle, INFRAX_SOL_SOCKET, INFRAX_SO_RCVTIMEO, &recv_tv, sizeof(recv_tv)) < 0) {
        char err_msg[256];
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Set receive timeout failed: %s (errno=%d)", 
            gInfraxCore.get_error_string(&gInfraxCore, err), err);
        return make_error(INFRAX_ERROR_NET_SET_TIMEOUT_FAILED_CODE, err_msg);
    }

    self->config.send_timeout_ms = send_timeout_ms;
    self->config.recv_timeout_ms = recv_timeout_ms;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_get_local_addr(InfraxNet* self, InfraxNetAddr* addr) {
    if (!self || !addr) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket or address pointer");
    
    InfraxSockAddrIn local_addr;
    size_t addr_len = sizeof(local_addr);
    
    if (gInfraxCore.socket_get_name(&gInfraxCore, self->native_handle, &local_addr, &addr_len) < 0) {
        char err_msg[256];
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Get local address failed: %s (errno=%d)", 
            gInfraxCore.get_error_string(&gInfraxCore, err), err);
        return make_error(INFRAX_ERROR_NET_OPTION_FAILED_CODE, err_msg);
    }

    addr->port = gInfraxCore.net_to_host16(&gInfraxCore, local_addr.sin_port);
    gInfraxCore.binary_to_ip(&gInfraxCore, &local_addr.sin_addr, addr->ip, sizeof(addr->ip));
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_get_peer_addr(InfraxNet* self, InfraxNetAddr* addr) {
    if (!self || !addr) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket or address pointer");
    if (!self->is_connected) return make_error(INFRAX_ERROR_NET_NOT_CONNECTED_CODE, "Socket is not connected");
    
    InfraxSockAddrIn peer_addr;
    size_t addr_len = sizeof(peer_addr);
    
    if (gInfraxCore.socket_get_peer(&gInfraxCore, self->native_handle, &peer_addr, &addr_len) < 0) {
        char err_msg[256];
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Get peer address failed: %s (errno=%d)", 
            gInfraxCore.get_error_string(&gInfraxCore, err), err);
        return make_error(INFRAX_ERROR_NET_OPTION_FAILED_CODE, err_msg);
    }

    addr->port = gInfraxCore.net_to_host16(&gInfraxCore, peer_addr.sin_port);
    gInfraxCore.binary_to_ip(&gInfraxCore, &peer_addr.sin_addr, addr->ip, sizeof(addr->ip));
    return INFRAX_ERROR_OK_STRUCT;
}

// Constructor and destructor
static InfraxNet* net_new(const InfraxNetConfig* config) {
    if (!config) {
        gInfraxCore.printf(&gInfraxCore, "net_new: config is NULL\n");
        return NULL;
    }

    // 获取内存管理器
    InfraxMemory* memory = get_memory_manager();
    if (!memory) {
        gInfraxCore.printf(&gInfraxCore, "net_new: failed to get memory manager\n");
        return NULL;
    }

    // 分配网络实例
    InfraxNet* net = (InfraxNet*)memory->alloc(memory, sizeof(InfraxNet));
    if (!net) {
        gInfraxCore.printf(&gInfraxCore, "net_new: failed to allocate network instance\n");
        return NULL;
    }

    // 初始化基本字段
    net->self = net;
    net->klass = &InfraxNetClass;
    net->config = *config;
    net->native_handle = -1;
    net->is_connected = INFRAX_FALSE;
    gInfraxCore.memset(&gInfraxCore, &net->local_addr, 0, sizeof(net->local_addr));
    gInfraxCore.memset(&gInfraxCore, &net->peer_addr, 0, sizeof(net->peer_addr));

    // 创建socket
    int domain = INFRAX_AF_INET;
    int type = config->is_udp ? INFRAX_SOCK_DGRAM : INFRAX_SOCK_STREAM;
    int protocol = config->is_udp ? INFRAX_IPPROTO_UDP : INFRAX_IPPROTO_TCP;

    gInfraxCore.printf(&gInfraxCore, "net_new: creating socket (domain=%d, type=%d, protocol=%d)\n", domain, type, protocol);
    intptr_t fd = gInfraxCore.socket_create(&gInfraxCore, domain, type, protocol);
    if (fd < 0) {
        gInfraxCore.printf(&gInfraxCore, "net_new: socket_create failed\n");
        memory->dealloc(memory, net);
        return NULL;
    }

    net->native_handle = fd;

    // 设置socket选项
    if (config->reuse_addr) {
        int reuse = 1;
        if (gInfraxCore.socket_set_option(&gInfraxCore, fd, INFRAX_SOL_SOCKET, INFRAX_SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            gInfraxCore.printf(&gInfraxCore, "net_new: failed to set SO_REUSEADDR\n");
            gInfraxCore.socket_close(&gInfraxCore, fd);
            memory->dealloc(memory, net);
            return NULL;
        }
    }

    // 设置非阻塞模式
    if (config->is_nonblocking) {
        int flags = gInfraxCore.fcntl(&gInfraxCore, fd, INFRAX_F_GETFL, 0);
        if (flags < 0) {
            gInfraxCore.printf(&gInfraxCore, "net_new: failed to get socket flags\n");
            gInfraxCore.socket_close(&gInfraxCore, fd);
            memory->dealloc(memory, net);
            return NULL;
        }
        if (gInfraxCore.fcntl(&gInfraxCore, fd, INFRAX_F_SETFL, flags | INFRAX_O_NONBLOCK) < 0) {
            gInfraxCore.printf(&gInfraxCore, "net_new: failed to set non-blocking mode\n");
            gInfraxCore.socket_close(&gInfraxCore, fd);
            memory->dealloc(memory, net);
            return NULL;
        }
    }

    // 设置超时
    if (config->send_timeout_ms > 0) {
        InfraxTimeVal tv;
        tv.tv_sec = config->send_timeout_ms / 1000;
        tv.tv_usec = (config->send_timeout_ms % 1000) * 1000;
        if (gInfraxCore.socket_set_option(&gInfraxCore, fd, INFRAX_SOL_SOCKET, INFRAX_SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
            gInfraxCore.printf(&gInfraxCore, "net_new: failed to set send timeout\n");
            gInfraxCore.socket_close(&gInfraxCore, fd);
            memory->dealloc(memory, net);
            return NULL;
        }
    }

    if (config->recv_timeout_ms > 0) {
        InfraxTimeVal tv;
        tv.tv_sec = config->recv_timeout_ms / 1000;
        tv.tv_usec = (config->recv_timeout_ms % 1000) * 1000;
        if (gInfraxCore.socket_set_option(&gInfraxCore, fd, INFRAX_SOL_SOCKET, INFRAX_SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            gInfraxCore.printf(&gInfraxCore, "net_new: failed to set receive timeout\n");
            gInfraxCore.socket_close(&gInfraxCore, fd);
            memory->dealloc(memory, net);
            return NULL;
        }
    }

    gInfraxCore.printf(&gInfraxCore, "net_new: socket created successfully (fd=%d)\n", (int)fd);
    return net;
}

static void net_free(InfraxNet* self) {
    if (!self) return;

    // 关闭socket
    if (self->native_handle >= 0) {
        InfraxError err = net_close(self);
        if (INFRAX_ERROR_IS_ERR(err)) {
            gInfraxCore.printf(&gInfraxCore, "Warning: net_close failed during free: %s\n", err.message);
        }
    }

    // 释放内存
    InfraxMemory* memory = get_memory_manager();
    if (memory) {
        memory->dealloc(memory, self);
    }
}

// Network class instance
InfraxNetClassType InfraxNetClass = {
    .new = net_new,
    .free = net_free,
    .bind = net_bind,
    .listen = net_listen,
    .accept = net_accept,
    .connect = net_connect,
    .send = net_send,
    .recv = net_recv,
    .sendto = net_sendto,
    .recvfrom = net_recvfrom,
    .set_option = net_set_option,
    .get_option = net_get_option,
    .set_nonblock = net_set_nonblocking,
    .set_timeout = net_set_timeout,
    .get_local_addr = net_get_local_addr,
    .get_peer_addr = net_get_peer_addr,
    .close = net_close,
    .shutdown = net_shutdown
};

// Private helper functions implementations
static InfraxError set_socket_option(intptr_t handle, int level, int option, const void* value, size_t len) {
    if (handle < 0) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket handle");
    if (!value) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid option value");

    int sys_level = map_socket_level(level);
    int sys_option = map_socket_option(option);

    if (gInfraxCore.socket_set_option(&gInfraxCore, handle, sys_level, sys_option, value, len) < 0) {
        int err = gInfraxCore.socket_get_error(&gInfraxCore, handle);
        char err_msg[256];
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Failed to set socket option: %s (errno=%d)", 
            gInfraxCore.get_error_string(&gInfraxCore, err), err);
        return make_error(INFRAX_ERROR_NET_SET_OPTION_FAILED_CODE, err_msg);
    }

    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError get_socket_option(intptr_t handle, int level, int option, void* value, size_t* len) {
    if (handle < 0) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket handle");
    if (!value || !len) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid option value or length pointer");

    int sys_level = map_socket_level(level);
    int sys_option = map_socket_option(option);

    if (gInfraxCore.socket_get_option(&gInfraxCore, handle, sys_level, sys_option, value, len) < 0) {
        int err = gInfraxCore.socket_get_error(&gInfraxCore, handle);
        char err_msg[256];
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Failed to get socket option: %s (errno=%d)", 
            gInfraxCore.get_error_string(&gInfraxCore, err), err);
        return make_error(INFRAX_ERROR_NET_GET_OPTION_FAILED_CODE, err_msg);
    }

    return INFRAX_ERROR_OK_STRUCT;
}

// Utility functions implementations
InfraxError infrax_net_addr_from_string(const char* ip, uint16_t port, InfraxNetAddr* addr) {
    if (!ip || !addr) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid IP or address pointer");

    InfraxInAddr inaddr;
    if (gInfraxCore.ip_to_binary(&gInfraxCore, ip, &inaddr, sizeof(inaddr)) <= 0) {
        return make_error(INFRAX_ERROR_NET_INVALID_ADDRESS_CODE, "Invalid IP address format");
    }

    gInfraxCore.strncpy(&gInfraxCore, addr->ip, ip, sizeof(addr->ip) - 1);
    addr->ip[sizeof(addr->ip) - 1] = '\0';
    addr->port = port;

    return INFRAX_ERROR_OK_STRUCT;
}

InfraxError infrax_net_addr_to_string(const InfraxNetAddr* addr, char* buffer, size_t size) {
    if (!addr || !buffer || size == 0) return INFRAX_ERROR_NET_INVALID_ARGUMENT;
    
    int result = gInfraxCore.snprintf(&gInfraxCore, buffer, size, "%s:%u", addr->ip, addr->port);
    if (result < 0 || (size_t)result >= size) {
        return INFRAX_ERROR_NET_INVALID_ARGUMENT;
    }
    
    return INFRAX_ERROR_OK_STRUCT;
}
