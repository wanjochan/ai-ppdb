#include "cosmopolitan.h"
#include "InfraxNet.h"
#include "InfraxCore.h"
#include "InfraxMemory.h"
/** TODO to remove cosmopolitan.h ref

1. 主要问题是缺少了一些系统头文件和定义：
   - <string.h> - 用于 memset, strncpy 等函数
   - <stdbool.h> - 用于 bool 类型
   - <sys/socket.h> - 用于 socket 相关的常量和结构体
   - <netinet/in.h> - 用于网络地址结构体
   - <arpa/inet.h> - 用于 inet_ntop 等函数
   - <unistd.h> - 用于 close 等函数
   - <fcntl.h> - 用于 fcntl 等函数
   - <sys/time.h> - 用于 timeval 结构体
   - <stdio.h> - 用于 fprintf 等函数

2. 需要替换的系统常量和类型：
   - SOL_SOCKET, SO_REUSEADDR 等 socket 选项
   - AF_INET, SOCK_STREAM 等协议族和类型
   - IPPROTO_TCP, IPPROTO_UDP 等协议
   - socklen_t, struct sockaddr_in 等类型
   - EAGAIN, EWOULDBLOCK 等错误码
   
 */

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
        case INFRAX_SO_RCVBUF:
            return SO_RCVBUF;
        case INFRAX_SO_SNDBUF:
            return SO_SNDBUF;
        case INFRAX_SO_ERROR:
            return SO_ERROR;
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

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = gInfraxCore.host_to_net16(&gInfraxCore, addr->port);
    
    // 验证 IP 地址格式
    if (inet_pton(AF_INET, addr->ip, &bind_addr.sin_addr) <= 0) {
        return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid IP address format");
    }

    if (gInfraxCore.socket_bind(&gInfraxCore, self->native_handle, &bind_addr, sizeof(bind_addr)) < 0) {
        char err_msg[256];
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Bind failed: %s (errno=%d)", strerror(err), err);
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
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Listen failed: %s (errno=%d)", strerror(err), err);
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
            sys_how = SHUT_RD;
            break;
        case INFRAX_SHUT_WR:
            sys_how = SHUT_WR;
            break;
        case INFRAX_SHUT_RDWR:
            sys_how = SHUT_RDWR;
            break;
        default:
            return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid shutdown mode");
    }
    
    if (gInfraxCore.socket_shutdown(&gInfraxCore, self->native_handle, sys_how) < 0) {
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        // 忽略某些特定的错误
        if (err == ENOTCONN) {
            // socket未连接，这是可以接受的
            return INFRAX_ERROR_OK_STRUCT;
        }
        
        char err_msg[256];
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Shutdown failed: %s (errno=%d)", strerror(err), err);
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
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Close failed: %s (errno=%d)", strerror(err), err);
        return make_error(INFRAX_ERROR_NET_SOCKET_FAILED_CODE, err_msg);
    }
    
    self->native_handle = -1;
    self->is_connected = INFRAX_FALSE;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_accept(InfraxNet* self, InfraxNet** client_socket, InfraxNetAddr* client_addr) {
    if (!self || !client_socket) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket or client socket pointer");
    if (self->config.is_udp) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "UDP socket cannot accept");

    struct sockaddr_in addr;
    size_t addr_len = sizeof(addr);
    intptr_t client_fd = gInfraxCore.socket_accept(&gInfraxCore, self->native_handle, &addr, &addr_len);
    
    if (client_fd < 0) {
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        if (err == EAGAIN || err == EWOULDBLOCK) {
            return make_error(INFRAX_ERROR_NET_WOULD_BLOCK_CODE, "Accept would block");
        }
        char err_msg[256];
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Accept failed: %s (errno=%d)", strerror(err), err);
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
        inet_ntop(AF_INET, &addr.sin_addr, client_addr->ip, sizeof(client_addr->ip));
        new_socket->peer_addr = *client_addr;
    }

    *client_socket = new_socket;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_connect(InfraxNet* self, const InfraxNetAddr* addr) {
    if (!self || !addr) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket or address");
    if (self->is_connected) return make_error(INFRAX_ERROR_NET_ALREADY_CONNECTED_CODE, "Socket is already connected");

    struct sockaddr_in connect_addr;
    gInfraxCore.memset(&gInfraxCore, &connect_addr, 0, sizeof(connect_addr));
    connect_addr.sin_family = AF_INET;
    connect_addr.sin_port = gInfraxCore.host_to_net16(&gInfraxCore, addr->port);
    
    if (inet_pton(AF_INET, addr->ip, &connect_addr.sin_addr) <= 0) {
        return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid IP address format");
    }

    // 保存原始的非阻塞状态
    bool was_nonblocking = self->config.is_nonblocking;
    
    // 设置为非阻塞模式
    InfraxError err = set_socket_nonblocking(self->native_handle, INFRAX_TRUE);
    if (INFRAX_ERROR_IS_ERR(err)) {
        return err;
    }

    // 尝试连接
    int connect_result = gInfraxCore.socket_connect(&gInfraxCore, self->native_handle, &connect_addr, sizeof(connect_addr));
    if (connect_result < 0) {
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        if (err != EINPROGRESS) {
            // 如果不是EINPROGRESS，说明是立即失败
            char err_msg[256];
            if (err == ECONNREFUSED) {
                gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Connection refused to %s:%d", addr->ip, addr->port);
            } else if (err == ETIMEDOUT) {
                gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Connection timed out to %s:%d", addr->ip, addr->port);
            } else if (err == ENETUNREACH) {
                gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Network unreachable for %s:%d", addr->ip, addr->port);
            } else if (err == EADDRNOTAVAIL) {
                gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Address not available: %s:%d", addr->ip, addr->port);
            } else {
                const char* err_str = strerror(err);
                if (!err_str) err_str = "Unknown error";
                gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Connect failed to %s:%d: %s (errno=%d)", 
                    addr->ip, addr->port, err_str, err);
            }
            return make_error(INFRAX_ERROR_NET_CONNECT_FAILED_CODE, err_msg);
        }

        // 使用select等待连接完成或超时
        fd_set write_fds;
        struct timeval tv;
        
        FD_ZERO(&write_fds);
        FD_SET(self->native_handle, &write_fds);
        
        tv.tv_sec = self->config.send_timeout_ms / 1000;
        tv.tv_usec = (self->config.send_timeout_ms % 1000) * 1000;

        int select_result = select(self->native_handle + 1, NULL, &write_fds, NULL, &tv);
        if (select_result < 0) {
            char err_msg[256];
            err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
            const char* err_str = strerror(err);
            if (!err_str) err_str = "Unknown error";
            gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Select failed while connecting to %s:%d: %s (errno=%d)", 
                addr->ip, addr->port, err_str, err);
            return make_error(INFRAX_ERROR_NET_CONNECT_FAILED_CODE, err_msg);
        } else if (select_result == 0) {
            char err_msg[256];
            gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Connection timed out while connecting to %s:%d", 
                addr->ip, addr->port);
            return make_error(INFRAX_ERROR_NET_CONNECT_FAILED_CODE, err_msg);
        }

        // 检查连接是否成功
        int socket_err;
        size_t socket_err_len = sizeof(socket_err);
        if (gInfraxCore.socket_get_option(&gInfraxCore, self->native_handle, SOL_SOCKET, SO_ERROR, &socket_err, &socket_err_len) < 0) {
            char err_msg[256];
            err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
            const char* err_str = strerror(err);
            if (!err_str) err_str = "Unknown error";
            gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Failed to get socket error while connecting to %s:%d: %s (errno=%d)", 
                addr->ip, addr->port, err_str, err);
            return make_error(INFRAX_ERROR_NET_CONNECT_FAILED_CODE, err_msg);
        }

        if (socket_err != 0) {
            char err_msg[256];
            const char* err_str = strerror(socket_err);
            if (!err_str) err_str = "Unknown error";
            gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Connect failed to %s:%d: %s (errno=%d)", 
                addr->ip, addr->port, err_str, socket_err);
            return make_error(INFRAX_ERROR_NET_CONNECT_FAILED_CODE, err_msg);
        }
    }

    // 恢复原始的非阻塞状态
    if (!was_nonblocking) {
        err = set_socket_nonblocking(self->native_handle, INFRAX_FALSE);
        if (INFRAX_ERROR_IS_ERR(err)) {
            return err;
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
        if (err == EAGAIN || err == EWOULDBLOCK) {
            *sent_size = 0;
            return make_error(INFRAX_ERROR_NET_WOULD_BLOCK_CODE, "Send would block");
        }
        char err_msg[256];
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Send failed: %s (errno=%d)", strerror(err), err);
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
        if (err == EAGAIN || err == EWOULDBLOCK) {
            *received_size = 0;
            return make_error(INFRAX_ERROR_NET_WOULD_BLOCK_CODE, "Receive would block");
        }
        char err_msg[256];
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Receive failed: %s (errno=%d)", strerror(err), err);
        return make_error(INFRAX_ERROR_NET_RECV_FAILED_CODE, err_msg);
    }

    *received_size = result;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_sendto(InfraxNet* self, const void* data, size_t size, size_t* sent_size, const InfraxNetAddr* addr) {
    if (!self || !data || !addr || !sent_size) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket, data, address or sent_size pointer");
    if (self->native_handle < 0) return make_error(INFRAX_ERROR_NET_INVALID_OPERATION_CODE, "Socket is not open");
    if (!self->config.is_udp) return make_error(INFRAX_ERROR_NET_INVALID_OPERATION_CODE, "Socket is not UDP");

    struct sockaddr_in send_addr;
    gInfraxCore.memset(&gInfraxCore, &send_addr, 0, sizeof(send_addr));
    send_addr.sin_family = AF_INET;
    send_addr.sin_port = gInfraxCore.host_to_net16(&gInfraxCore, addr->port);
    
    if (inet_pton(AF_INET, addr->ip, &send_addr.sin_addr) <= 0) {
        return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid IP address format");
    }

    ssize_t result = gInfraxCore.socket_send(&gInfraxCore, self->native_handle, data, size, 0);
    if (result < 0) {
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        if (err == EAGAIN || err == EWOULDBLOCK) {
            *sent_size = 0;
            return make_error(INFRAX_ERROR_NET_WOULD_BLOCK_CODE, "Send would block");
        }
        char err_msg[256];
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Send failed: %s (errno=%d)", strerror(err), err);
        return make_error(INFRAX_ERROR_NET_SEND_FAILED_CODE, err_msg);
    }

    *sent_size = result;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_recvfrom(InfraxNet* self, void* buffer, size_t size, size_t* received_size, InfraxNetAddr* from_addr) {
    if (!self || !buffer || !from_addr || !received_size) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket, buffer, from_addr or received_size pointer");
    if (self->native_handle < 0) return make_error(INFRAX_ERROR_NET_INVALID_OPERATION_CODE, "Socket is not open");
    if (!self->config.is_udp) return make_error(INFRAX_ERROR_NET_INVALID_OPERATION_CODE, "Socket is not UDP");

    struct sockaddr_in recv_addr;
    socklen_t addr_len = sizeof(recv_addr);
    gInfraxCore.memset(&gInfraxCore, &recv_addr, 0, sizeof(recv_addr));

    ssize_t result = gInfraxCore.socket_recv(&gInfraxCore, self->native_handle, buffer, size, 0);
    if (result < 0) {
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        if (err == EAGAIN || err == EWOULDBLOCK) {
            *received_size = 0;
            return make_error(INFRAX_ERROR_NET_WOULD_BLOCK_CODE, "Receive would block");
        }
        char err_msg[256];
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Receive failed: %s (errno=%d)", strerror(err), err);
        return make_error(INFRAX_ERROR_NET_RECV_FAILED_CODE, err_msg);
    }

    // 填充发送方地址信息
    from_addr->port = gInfraxCore.net_to_host16(&gInfraxCore, recv_addr.sin_port);
    inet_ntop(AF_INET, &recv_addr.sin_addr, from_addr->ip, sizeof(from_addr->ip));

    *received_size = result;
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
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Set socket option failed: %s (errno=%d)", strerror(err), err);
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
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Get socket option failed: %s (errno=%d)", strerror(err), err);
        return make_error(INFRAX_ERROR_NET_GET_OPTION_FAILED_CODE, err_msg);
    }

    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError set_socket_nonblocking(intptr_t handle, InfraxBool nonblocking) {
    if (handle < 0) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid socket handle");

    int flags = nonblocking ? 1 : 0;
    if (gInfraxCore.socket_set_option(&gInfraxCore, handle, SOL_SOCKET, O_NONBLOCK, &flags, sizeof(flags)) < 0) {
        char err_msg[256];
        int err = gInfraxCore.socket_get_error(&gInfraxCore, handle);
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Set nonblocking failed: %s (errno=%d)", strerror(err), err);
        return make_error(INFRAX_ERROR_NET_SET_NONBLOCKING_FAILED_CODE, err_msg);
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

    struct timeval send_tv = {
        .tv_sec = send_timeout_ms / 1000,
        .tv_usec = (send_timeout_ms % 1000) * 1000
    };

    struct timeval recv_tv = {
        .tv_sec = recv_timeout_ms / 1000,
        .tv_usec = (recv_timeout_ms % 1000) * 1000
    };

    // 设置发送超时
    if (gInfraxCore.socket_set_option(&gInfraxCore, self->native_handle, SOL_SOCKET, SO_SNDTIMEO, &send_tv, sizeof(send_tv)) < 0) {
        char err_msg[256];
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Set send timeout failed: %s (errno=%d)", strerror(err), err);
        return make_error(INFRAX_ERROR_NET_SET_TIMEOUT_FAILED_CODE, err_msg);
    }

    // 设置接收超时
    if (gInfraxCore.socket_set_option(&gInfraxCore, self->native_handle, SOL_SOCKET, SO_RCVTIMEO, &recv_tv, sizeof(recv_tv)) < 0) {
        char err_msg[256];
        int err = gInfraxCore.socket_get_error(&gInfraxCore, self->native_handle);
        gInfraxCore.snprintf(&gInfraxCore, err_msg, sizeof(err_msg), "Set receive timeout failed: %s (errno=%d)", strerror(err), err);
        return make_error(INFRAX_ERROR_NET_SET_TIMEOUT_FAILED_CODE, err_msg);
    }

    self->config.send_timeout_ms = send_timeout_ms;
    self->config.recv_timeout_ms = recv_timeout_ms;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError net_get_local_addr(InfraxNet* self, InfraxNetAddr* addr) {
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

static InfraxError net_get_peer_addr(InfraxNet* self, InfraxNetAddr* addr) {
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
static InfraxNet* net_new(const InfraxNetConfig* config) {
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
    InfraxNet* self = get_memory_manager()->alloc(get_memory_manager(), sizeof(InfraxNet));
    if (!self) {
        close(fd);
        return NULL;
    }
    
    // 清零所有内存
    memset(self, 0, sizeof(InfraxNet));
    
    self->self = self;
    self->klass = &InfraxNetClass;

    // Initialize socket
    self->config = *config;
    self->native_handle = fd;
    self->is_connected = INFRAX_FALSE;
    
    // Initialize addresses
    memset(&self->local_addr, 0, sizeof(self->local_addr));
    memset(&self->peer_addr, 0, sizeof(self->peer_addr));
    
    // Set socket options
    if (config->reuse_addr) {
        int reuse = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            net_free(self);
            return NULL;
        }
    }
    
    // Set non-blocking mode
    if (config->is_nonblocking) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            net_free(self);
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
                net_free(self);
                return NULL;
            }
        }
        
        if (config->recv_timeout_ms > 0) {
            if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout)) < 0) {
                net_free(self);
                return NULL;
            }
        }
    }
    
    return self;
}

static void net_free(InfraxNet* self) {
    if (!self) return;
    
    if (self->native_handle >= 0) {
        // 使用net_close来优雅地关闭socket
        InfraxError err = net_close(self);
        if (INFRAX_ERROR_IS_ERR(err)) {
            fprintf(stderr, "Warning: net_close failed during free: %s\n", err.message);
        }
    }
    
    get_memory_manager()->dealloc(get_memory_manager(), self);
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

// Utility functions implementations
InfraxError infrax_net_addr_from_string(const char* ip, uint16_t port, InfraxNetAddr* addr) {
    if (!ip || !addr) return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid arguments: NULL pointer");
    
    struct in_addr inaddr;
    if (inet_pton(AF_INET, ip, &inaddr) <= 0) {
        return make_error(INFRAX_ERROR_NET_INVALID_ARGUMENT_CODE, "Invalid IP address format");
    }

    strncpy(addr->ip, ip, sizeof(addr->ip) - 1);
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
