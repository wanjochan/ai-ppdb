/*
 * infra_net.h - Network Operations Interface
 */

#ifndef INFRA_NET_H
#define INFRA_NET_H

#include "internal/infra/infra_error.h"
#include "internal/infra/infra_core.h"

// 网络地址结构
typedef struct infra_net_addr {
    const char* host;  // 主机名或IP地址
    uint16_t port;     // 端口号
} infra_net_addr_t;

// 套接字结构
struct infra_socket {
    int fd;           // 文件描述符
    bool is_udp;      // 是否为UDP套接字
    void* handle;     // 平台相关的句柄
    void* overlapped; // 平台相关的重叠IO结构
};

// 套接字句柄类型
typedef struct infra_socket* infra_socket_t;

// accept4 标志位
#define INFRA_NET_ACCEPT_NONBLOCK 0x01  // 设置非阻塞
#define INFRA_NET_ACCEPT_CLOEXEC  0x02  // 设置 close-on-exec

// 服务端接口
infra_error_t infra_net_create(infra_socket_t* sock, bool is_udp, const infra_config_t* config);
infra_error_t infra_net_bind(infra_socket_t sock, const infra_net_addr_t* addr);
infra_error_t infra_net_listen(infra_socket_t sock);
infra_error_t infra_net_accept(infra_socket_t sock, infra_socket_t* client, infra_net_addr_t* client_addr);

//ref @cosmomplitan special
infra_error_t infra_net_accept4(infra_socket_t sock, infra_socket_t* client, infra_net_addr_t* client_addr, int flags);

// 客户端接口
infra_error_t infra_net_connect(const infra_net_addr_t* addr, infra_socket_t* sock, const infra_config_t* config);

// 通用操作
infra_error_t infra_net_close(infra_socket_t sock);
infra_error_t infra_net_set_nonblock(infra_socket_t sock, bool enable);
infra_error_t infra_net_set_keepalive(infra_socket_t sock, bool enable);
infra_error_t infra_net_set_reuseaddr(infra_socket_t sock, bool enable);
infra_error_t infra_net_set_nodelay(infra_socket_t sock, bool enable);
infra_error_t infra_net_set_timeout(infra_socket_t sock, uint32_t timeout_ms);
infra_error_t infra_net_getsockname(infra_socket_t sock, infra_net_addr_t* addr);
infra_error_t infra_net_getpeername(infra_socket_t sock, infra_net_addr_t* addr);

// 数据传输
infra_error_t infra_net_send(infra_socket_t sock, const void* buf, size_t len, size_t* sent);
infra_error_t infra_net_recv(infra_socket_t sock, void* buf, size_t len, size_t* received);

// UDP 操作
infra_error_t infra_net_sendto(infra_socket_t sock, const void* buf, size_t len, const infra_net_addr_t* addr, size_t* sent);
infra_error_t infra_net_recvfrom(infra_socket_t sock, void* buf, size_t len, infra_net_addr_t* addr, size_t* received);

// 地址转换
infra_error_t infra_net_resolve(const char* host, infra_net_addr_t* addr);
infra_error_t infra_net_addr_to_str(const infra_net_addr_t* addr, char* buf, size_t size);

// socket fd or handle
int infra_net_get_fd(infra_socket_t sock);

// 刷新发送缓冲区
infra_error_t infra_net_flush(infra_socket_t socket);

// 关闭方式
typedef enum {
    INFRA_NET_SHUTDOWN_READ = 0,    // 关闭读
    INFRA_NET_SHUTDOWN_WRITE = 1,   // 关闭写
    INFRA_NET_SHUTDOWN_BOTH = 2     // 关闭读写
} infra_net_shutdown_how_t;

// 关闭socket的读写
infra_error_t infra_net_shutdown(infra_socket_t socket, infra_net_shutdown_how_t how);

#endif /* INFRA_NET_H */ 
