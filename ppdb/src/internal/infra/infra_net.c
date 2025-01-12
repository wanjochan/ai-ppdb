/*
 * infra_net.c - Network Infrastructure Layer Implementation
 */

#include "cosmopolitan.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_memory.h"

// 创建socket
static infra_error_t create_socket(bool is_udp, infra_socket_t* sock) {
    *sock = (infra_socket_t)infra_malloc(sizeof(struct infra_socket));
    if (*sock == NULL) {
        return INFRA_ERROR_NO_MEMORY;
    }

    (*sock)->fd = socket(AF_INET, is_udp ? SOCK_DGRAM : SOCK_STREAM, 0);
    if ((*sock)->fd == -1) {
        infra_free(*sock);
        *sock = NULL;
        return INFRA_ERROR_SYSTEM;
    }

    (*sock)->is_udp = is_udp;
    (*sock)->handle = (void*)(intptr_t)(*sock)->fd;
    (*sock)->overlapped = NULL;

    return INFRA_OK;
}

// 实现网络函数
infra_error_t infra_net_listen(const infra_net_addr_t* addr, infra_socket_t* sock) {
    if (addr == NULL || sock == NULL) {
        return INFRA_ERROR_INVALID;
    }

    // 创建socket
    infra_error_t err = create_socket(false, sock);
    if (err != INFRA_OK) {
        return err;
    }

    // 设置地址结构
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(addr->port);
    
    // 将IP地址字符串转换为网络字节序
    if (inet_pton(AF_INET, addr->host, &server_addr.sin_addr) != 1) {
        infra_net_close(*sock);
        return INFRA_ERROR_INVALID;
    }

    // 绑定地址
    if (bind((*sock)->fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        infra_net_close(*sock);
        return INFRA_ERROR_SYSTEM;
    }

    // 开始监听
    if (listen((*sock)->fd, SOMAXCONN) == -1) {
        infra_net_close(*sock);
        return INFRA_ERROR_SYSTEM;
    }

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

infra_error_t infra_net_connect(const infra_net_addr_t* addr, infra_socket_t* sock) {
    if (addr == NULL || sock == NULL) {
        return INFRA_ERROR_INVALID;
    }

    // 创建socket
    infra_error_t err = create_socket(false, sock);
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
        if (errno == EINPROGRESS || errno == EWOULDBLOCK) {
            return INFRA_ERROR_WOULD_BLOCK;
        }
        infra_net_close(*sock);
        return INFRA_ERROR_SYSTEM;
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

infra_error_t infra_net_udp_socket(infra_socket_t* sock) {
    if (sock == NULL) {
        return INFRA_ERROR_INVALID;
    }
    return create_socket(true, sock);
}

infra_error_t infra_net_udp_bind(const infra_net_addr_t* addr, infra_socket_t* sock) {
    if (addr == NULL || sock == NULL) {
        return INFRA_ERROR_INVALID;
    }

    // 创建UDP socket
    infra_error_t err = create_socket(true, sock);
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

// 获取文件描述符
int infra_net_get_fd(infra_socket_t sock) {
    if (!sock) {
        return -1;  // 无效的套接字
    }
    return (int)(intptr_t)sock->handle;  // 使用 handle 字段
} 