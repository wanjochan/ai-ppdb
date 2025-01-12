/*
 * infra_net.c - Network Infrastructure Layer Implementation
 */

#include "cosmopolitan.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_memory.h"
#include "internal/infra/infra_platform.h"
#include "internal/infra/infra_core.h"

static infra_error_t create_socket(infra_socket_t* sock, bool is_udp, const infra_config_t* config, bool nonblocking) {
    if (!sock || !config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    *sock = (infra_socket_t)infra_malloc(sizeof(struct infra_socket));
    if (!*sock) {
        return INFRA_ERROR_NO_MEMORY;
    }

    (*sock)->is_udp = is_udp;
    (*sock)->overlapped = NULL;

    int type = is_udp ? SOCK_DGRAM : SOCK_STREAM;
    int protocol = is_udp ? IPPROTO_UDP : IPPROTO_TCP;

    (*sock)->fd = socket(AF_INET, type, protocol);
    if ((*sock)->fd == -1) {
        infra_free(*sock);
        *sock = NULL;
        return INFRA_ERROR_SYSTEM;
    }

    // 根据参数设置非阻塞模式
    // 注意：
    // 1. 监听套接字总是非阻塞，以支持多路复用
    // 2. 客户端套接字根据config->flags决定
    // 3. accept返回的套接字继承监听套接字的设置
    if (nonblocking) {
        int flags = fcntl((*sock)->fd, F_GETFL, 0);
        if (flags == -1) {
            close((*sock)->fd);
            infra_free(*sock);
            *sock = NULL;
            return INFRA_ERROR_SYSTEM;
        }
        if (fcntl((*sock)->fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            close((*sock)->fd);
            infra_free(*sock);
            *sock = NULL;
            return INFRA_ERROR_SYSTEM;
        }
    }

    infra_printf("Socket created successfully, fd: %d, is_udp: %d, nonblocking: %d\n", 
                 (*sock)->fd, is_udp, nonblocking);
    return INFRA_OK;
}

// 实现网络函数
infra_error_t infra_net_listen(const infra_net_addr_t* addr, infra_socket_t* sock, const infra_config_t* config) {
    if (addr == NULL || sock == NULL || config == NULL) {
        return INFRA_ERROR_INVALID;
    }

    infra_printf("Creating socket for listen on %s:%d\n", addr->host, addr->port);

    // 创建socket - 监听socket总是非阻塞以支持多路复用
    // 注意：这是为了支持 mux 模块的多路复用功能，即使在阻塞模式下也是如此
    infra_error_t err = create_socket(sock, false, config, true);
    if (err != INFRA_OK) {
        infra_printf("Failed to create socket: %d\n", err);
        return err;
    }

    // 在绑定前设置 SO_REUSEADDR
    int optval = 1;
    if (setsockopt((*sock)->fd, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval)) == -1) {
        infra_printf("setsockopt SO_REUSEADDR failed with errno: %d\n", errno);
        infra_net_close(*sock);
        return INFRA_ERROR_SYSTEM;
    }
    infra_printf("SO_REUSEADDR set successfully\n");

    // 设置地址结构
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(addr->port);
    
    // 使用 INADDR_ANY 而不是特定的 IP
    if (infra_strcmp(addr->host, "127.0.0.1") == 0) {
        infra_printf("Using INADDR_LOOPBACK for localhost\n");
        server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    } else {
        infra_printf("Using INADDR_ANY for %s\n", addr->host);
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    infra_printf("Binding socket (fd: %d, family: %d, port: %d, addr: 0x%x)\n", 
                 (*sock)->fd, 
                 server_addr.sin_family,
                 ntohs(server_addr.sin_port),
                 ntohl(server_addr.sin_addr.s_addr));

    // 绑定地址
    if (bind((*sock)->fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        int bind_errno = errno;
        infra_printf("bind failed with errno: %d, fd: %d\n", bind_errno, (*sock)->fd);
        infra_net_close(*sock);
        return INFRA_ERROR_SYSTEM;
    }

    // 开始监听
    if (listen((*sock)->fd, SOMAXCONN) == -1) {
        infra_printf("listen failed with errno: %d, fd: %d\n", errno, (*sock)->fd);
        infra_net_close(*sock);
        return INFRA_ERROR_SYSTEM;
    }

    infra_printf("Successfully listening on %s:%d, fd: %d\n", addr->host, addr->port, (*sock)->fd);
    return INFRA_OK;
}

infra_error_t infra_net_accept(infra_socket_t sock, infra_socket_t* client, infra_net_addr_t* client_addr) {
    if (sock == NULL || client == NULL) {
        return INFRA_ERROR_INVALID;
    }

    // 创建新的socket结构
    *client = (infra_socket_t)infra_malloc(sizeof(struct infra_socket));
    if (*client == NULL) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // 接受连接
    struct sockaddr_in addr = {0};
    socklen_t addr_len = sizeof(addr);
    (*client)->fd = accept(sock->fd, (struct sockaddr*)&addr, &addr_len);
    if ((*client)->fd == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            infra_free(*client);
            *client = NULL;
            return INFRA_ERROR_WOULD_BLOCK;
        }
        infra_free(*client);
        *client = NULL;
        return INFRA_ERROR_SYSTEM;
    }

    (*client)->is_udp = false;

    // 继承服务器套接字的非阻塞设置
    // 注意：这确保了accept返回的客户端套接字与服务器套接字有相同的阻塞行为
    // 这对于 mux 模块的正确工作是必要的
    int flags = fcntl(sock->fd, F_GETFL, 0);
    if (flags != -1 && (flags & O_NONBLOCK)) {
        if (fcntl((*client)->fd, F_SETFL, flags) == -1) {
            infra_net_close(*client);
            *client = NULL;
            return INFRA_ERROR_SYSTEM;
        }
    }

    // 如果需要，填充客户端地址信息
    if (client_addr != NULL) {
        client_addr->port = ntohs(addr.sin_port);
        char* ip_str = (char*)infra_malloc(INET_ADDRSTRLEN);
        if (ip_str == NULL) {
            infra_net_close(*client);
            *client = NULL;
            return INFRA_ERROR_NO_MEMORY;
        }
        if (inet_ntop(AF_INET, &addr.sin_addr, ip_str, INET_ADDRSTRLEN) != NULL) {
            client_addr->host = ip_str;
        } else {
            infra_free(ip_str);
            infra_net_close(*client);
            *client = NULL;
            return INFRA_ERROR_SYSTEM;
        }
    }

    return INFRA_OK;
}

infra_error_t infra_net_connect(const infra_net_addr_t* addr, infra_socket_t* sock, const infra_config_t* config) {
    if (addr == NULL || sock == NULL || config == NULL) {
        return INFRA_ERROR_INVALID;
    }

    // 创建socket - 根据配置决定是否非阻塞
    // 注意：这里使用 config->net.flags 中的 INFRA_CONFIG_FLAG_NONBLOCK 标志来决定
    // 这允许应用程序自由选择阻塞或非阻塞模式
    bool nonblocking = config->net.flags & INFRA_CONFIG_FLAG_NONBLOCK;
    infra_error_t err = create_socket(sock, false, config, nonblocking);
    if (err != INFRA_OK) {
        return err;
    }

    // 设置地址结构
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(addr->port);
    if (inet_pton(AF_INET, addr->host, &server_addr.sin_addr) != 1) {
        infra_net_close(*sock);
        return INFRA_ERROR_INVALID;
    }

    // 连接服务器
    if (connect((*sock)->fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        int connect_errno = errno;
        if (connect_errno == EINPROGRESS || connect_errno == EWOULDBLOCK) {
            if (nonblocking) {
                // 非阻塞模式：立即返回 WOULD_BLOCK，由调用者处理后续操作
                return INFRA_ERROR_WOULD_BLOCK;
            }
            
            // 阻塞模式：等待连接完成或超时
            // 注意：即使在阻塞模式下，我们也使用select来实现超时功能
            fd_set write_fds;
            struct timeval tv;
            FD_ZERO(&write_fds);
            FD_SET((*sock)->fd, &write_fds);
            
            // 使用配置中的超时时间，如果未设置则使用默认值
            uint32_t timeout_ms = config->net.connect_timeout_ms > 0 ? 
                                config->net.connect_timeout_ms : 1000;  // 默认1秒
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;

            int ret = select((*sock)->fd + 1, NULL, &write_fds, NULL, &tv);
            if (ret == 0) {
                // 超时
                infra_net_close(*sock);
                return INFRA_ERROR_TIMEOUT;
            } else if (ret < 0) {
                // select错误
                infra_net_close(*sock);
                return INFRA_ERROR_SYSTEM;
            }

            // 检查连接是否成功
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt((*sock)->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
                infra_net_close(*sock);
                return INFRA_ERROR_SYSTEM;
            }

            if (error != 0) {
                infra_net_close(*sock);
                return INFRA_ERROR_SYSTEM;
            }
        } else {
            infra_net_close(*sock);
            return INFRA_ERROR_SYSTEM;
        }
    }

    // 根据配置设置其他选项
    if (config->net.flags & INFRA_CONFIG_FLAG_NODELAY) {
        err = infra_net_set_nodelay(*sock, true);
        if (err != INFRA_OK) {
            infra_net_close(*sock);
            return err;
        }
    }

    if (config->net.flags & INFRA_CONFIG_FLAG_KEEPALIVE) {
        err = infra_net_set_keepalive(*sock, true);
        if (err != INFRA_OK) {
            infra_net_close(*sock);
            return err;
        }
    }

    // 设置读写超时
    if (config->net.read_timeout_ms > 0 || config->net.write_timeout_ms > 0) {
        struct timeval tv_read = {0}, tv_write = {0};
        
        if (config->net.read_timeout_ms > 0) {
            tv_read.tv_sec = config->net.read_timeout_ms / 1000;
            tv_read.tv_usec = (config->net.read_timeout_ms % 1000) * 1000;
            if (setsockopt((*sock)->fd, SOL_SOCKET, SO_RCVTIMEO, &tv_read, sizeof(tv_read)) == -1) {
                infra_net_close(*sock);
                return INFRA_ERROR_SYSTEM;
            }
        }
        
        if (config->net.write_timeout_ms > 0) {
            tv_write.tv_sec = config->net.write_timeout_ms / 1000;
            tv_write.tv_usec = (config->net.write_timeout_ms % 1000) * 1000;
            if (setsockopt((*sock)->fd, SOL_SOCKET, SO_SNDTIMEO, &tv_write, sizeof(tv_write)) == -1) {
                infra_net_close(*sock);
                return INFRA_ERROR_SYSTEM;
            }
        }
    }

    return INFRA_OK;
}

infra_error_t infra_net_close(infra_socket_t sock) {
    if (sock == NULL) {
        return INFRA_ERROR_INVALID;
    }

    close(sock->fd);
    infra_free(sock);
    return INFRA_OK;
}

infra_error_t infra_net_set_nonblock(infra_socket_t sock, bool enable) {
    if (sock == NULL) {
        return INFRA_ERROR_INVALID;
    }

    int flags = fcntl(sock->fd, F_GETFL, 0);
    if (flags == -1) {
        return INFRA_ERROR_SYSTEM;
    }
    flags = enable ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (fcntl(sock->fd, F_SETFL, flags) == -1) {
        return INFRA_ERROR_SYSTEM;
    }

    return INFRA_OK;
}

infra_error_t infra_net_set_keepalive(infra_socket_t sock, bool enable) {
    if (sock == NULL) {
        return INFRA_ERROR_INVALID;
    }

    int optval = enable ? 1 : 0;
    if (setsockopt(sock->fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1) {
        return INFRA_ERROR_SYSTEM;
    }
    return INFRA_OK;
}

infra_error_t infra_net_set_reuseaddr(infra_socket_t sock, bool enable) {
    if (sock == NULL) {
        return INFRA_ERROR_INVALID;
    }

    int optval = enable ? 1 : 0;
    if (setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        return INFRA_ERROR_SYSTEM;
    }
    return INFRA_OK;
}

infra_error_t infra_net_set_nodelay(infra_socket_t sock, bool enable) {
    if (sock == NULL) {
        return INFRA_ERROR_INVALID;
    }

    int optval = enable ? 1 : 0;
    if (setsockopt(sock->fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) == -1) {
        return INFRA_ERROR_SYSTEM;
    }
    return INFRA_OK;
}

infra_error_t infra_net_set_timeout(infra_socket_t sock, uint32_t timeout_ms) {
    if (sock == NULL) {
        return INFRA_ERROR_INVALID;
    }

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    // 设置发送和接收超时
    if (setsockopt(sock->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1 ||
        setsockopt(sock->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == -1) {
        return INFRA_ERROR_SYSTEM;
    }

    return INFRA_OK;
}

infra_error_t infra_net_send(infra_socket_t sock, const void* buf, size_t len, size_t* sent) {
    if (sock == NULL || buf == NULL || sent == NULL) {
        return INFRA_ERROR_INVALID;
    }

    ssize_t ret = send(sock->fd, buf, len, 0);
    if (ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *sent = 0;
            return INFRA_ERROR_WOULD_BLOCK;
        }
        if (errno == ETIMEDOUT) {
            return INFRA_ERROR_TIMEOUT;
        }
        return INFRA_ERROR_SYSTEM;
    }

    *sent = ret;
    return INFRA_OK;
}

infra_error_t infra_net_recv(infra_socket_t sock, void* buf, size_t len, size_t* received) {
    if (sock == NULL || buf == NULL || received == NULL) {
        return INFRA_ERROR_INVALID;
    }

    ssize_t ret = recv(sock->fd, buf, len, 0);
    if (ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ETIMEDOUT) {
            *received = 0;
            return INFRA_ERROR_TIMEOUT;
        }
        return INFRA_ERROR_SYSTEM;
    }
    if (ret == 0) {
        return INFRA_ERROR_CLOSED;
    }

    *received = ret;
    return INFRA_OK;
}

infra_error_t infra_net_udp_socket(infra_socket_t* sock, const infra_config_t* config) {
    if (sock == NULL || config == NULL) {
        return INFRA_ERROR_INVALID;
    }
    return create_socket(sock, true, config, false);
}

infra_error_t infra_net_udp_bind(const infra_net_addr_t* addr, infra_socket_t* sock, const infra_config_t* config) {
    if (addr == NULL || sock == NULL || config == NULL) {
        return INFRA_ERROR_INVALID;
    }

    // 创建socket - UDP默认阻塞
    infra_error_t err = create_socket(sock, true, config, false);
    if (err != INFRA_OK) {
        return err;
    }

    // 设置地址结构
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(addr->port);
    if (inet_pton(AF_INET, addr->host, &server_addr.sin_addr) != 1) {
        infra_net_close(*sock);
        return INFRA_ERROR_INVALID;
    }

    // 绑定地址
    if (bind((*sock)->fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        infra_net_close(*sock);
        return INFRA_ERROR_SYSTEM;
    }

    return INFRA_OK;
}

infra_error_t infra_net_sendto(infra_socket_t sock, const void* buf, size_t len, 
                              const infra_net_addr_t* addr, size_t* sent) {
    if (sock == NULL || buf == NULL || addr == NULL || sent == NULL) {
        return INFRA_ERROR_INVALID;
    }

    struct sockaddr_in dest_addr = {0};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(addr->port);
    if (inet_pton(AF_INET, addr->host, &dest_addr.sin_addr) != 1) {
        return INFRA_ERROR_INVALID;
    }

    ssize_t ret = sendto(sock->fd, buf, len, 0, 
                        (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    if (ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *sent = 0;
            return INFRA_ERROR_WOULD_BLOCK;
        }
        if (errno == ETIMEDOUT) {
            return INFRA_ERROR_TIMEOUT;
        }
        return INFRA_ERROR_SYSTEM;
    }

    *sent = ret;
    return INFRA_OK;
}

infra_error_t infra_net_recvfrom(infra_socket_t sock, void* buf, size_t len,
                                infra_net_addr_t* addr, size_t* received) {
    if (sock == NULL || buf == NULL || received == NULL) {
        return INFRA_ERROR_INVALID;
    }

    struct sockaddr_in src_addr = {0};
    socklen_t addr_len = sizeof(src_addr);
    
    ssize_t ret = recvfrom(sock->fd, buf, len, 0,
                          (struct sockaddr*)&src_addr, &addr_len);
    if (ret == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            *received = 0;
            return INFRA_ERROR_WOULD_BLOCK;
        }
        if (errno == ETIMEDOUT) {
            return INFRA_ERROR_TIMEOUT;
        }
        return INFRA_ERROR_SYSTEM;
    }

    *received = ret;

    // 如果需要，填充源地址信息
    if (addr != NULL) {
        addr->port = ntohs(src_addr.sin_port);
        char* ip_str = (char*)infra_malloc(INET_ADDRSTRLEN);
        if (ip_str == NULL) {
            return INFRA_ERROR_NO_MEMORY;
        }
        if (inet_ntop(AF_INET, &src_addr.sin_addr, ip_str, INET_ADDRSTRLEN) != NULL) {
            addr->host = ip_str;
        } else {
            infra_free(ip_str);
            return INFRA_ERROR_SYSTEM;
        }
    }

    return INFRA_OK;
}

infra_error_t infra_net_resolve(const char* host, infra_net_addr_t* addr) {
    if (host == NULL || addr == NULL) {
        return INFRA_ERROR_INVALID;
    }

    // 尝试直接解析IP地址
    struct in_addr ip_addr;
    if (inet_pton(AF_INET, host, &ip_addr) == 1) {
        char* ip_str = (char*)infra_malloc(INET_ADDRSTRLEN);
        if (ip_str == NULL) {
            return INFRA_ERROR_NO_MEMORY;
        }
        if (inet_ntop(AF_INET, &ip_addr, ip_str, INET_ADDRSTRLEN) != NULL) {
            addr->host = ip_str;
            return INFRA_OK;
        }
        infra_free(ip_str);
        return INFRA_ERROR_SYSTEM;
    }

    // 使用DNS解析
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    struct addrinfo* result;
    if (getaddrinfo(host, NULL, &hints, &result) != 0) {
        return INFRA_ERROR_SYSTEM;
    }

    // 获取第一个IPv4地址
    for (struct addrinfo* rp = result; rp != NULL; rp = rp->ai_next) {
        if (rp->ai_family == AF_INET) {
            struct sockaddr_in* addr_in = (struct sockaddr_in*)rp->ai_addr;
            char* ip_str = (char*)infra_malloc(INET_ADDRSTRLEN);
            if (ip_str == NULL) {
                freeaddrinfo(result);
                return INFRA_ERROR_NO_MEMORY;
            }
            if (inet_ntop(AF_INET, &addr_in->sin_addr, ip_str, INET_ADDRSTRLEN) != NULL) {
                addr->host = ip_str;
                freeaddrinfo(result);
                return INFRA_OK;
            }
            infra_free(ip_str);
        }
    }

    freeaddrinfo(result);
    return INFRA_ERROR_NOT_FOUND;
}

infra_error_t infra_net_addr_to_str(const infra_net_addr_t* addr, char* buf, size_t size) {
    if (addr == NULL || buf == NULL || size == 0) {
        return INFRA_ERROR_INVALID;
    }

    int ret = snprintf(buf, size, "%s:%u", addr->host, addr->port);
    if (ret < 0 || (size_t)ret >= size) {
        return INFRA_ERROR_INVALID;
    }

    return INFRA_OK;
}

int infra_net_get_fd(infra_socket_t sock) {
    if (!sock) {
        return -1;  // 无效的套接字
    }
    return sock->fd;
} 
