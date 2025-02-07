#ifndef PEER_MEMKV_H_
#define PEER_MEMKV_H_

#include "internal/peer/peer_service.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/poly/poly_db.h"
#include "internal/poly/poly_poll.h"

// 增加缓冲区大小到 1MB
#define MEMKV_CONN_BUFFER_SIZE (1024 * 1024)

// 服务状态
typedef struct {
    char host[64];                // 监听地址
    int port;                     // 监听端口
    char engine[32];              // 存储引擎
    char plugin[256];             // 插件路径
    bool running;                 // 运行状态
    poly_poll_context_t* ctx;     // 轮询上下文
    infra_mutex_t mutex;          // 互斥锁
} memkv_state_t;

// 连接状态
typedef struct {
    infra_socket_t sock;          // 客户端 socket
    poly_db_t* store;             // 数据库连接
    char client_addr[64];         // 客户端地址
    char* rx_buf;                 // 接收缓冲区
    size_t rx_len;                // 缓冲区中的数据长度
    size_t rx_pos;                // 当前处理位置
    bool should_close;            // 是否应该关闭连接
    bool is_closing;              // 是否正在关闭连接
    bool is_initialized;          // 是否已初始化
    time_t last_active_time;      // 最后活动时间
    uint64_t total_commands;      // 总命令数
    uint64_t failed_commands;     // 失败命令数
} memkv_conn_t;

// Service interface functions
infra_error_t memkv_init(void);
infra_error_t memkv_cleanup(void);
infra_error_t memkv_start(void);
infra_error_t memkv_stop(void);
infra_error_t memkv_cmd_handler(const char* cmd, char* response, size_t size);

// Get memkv service instance
peer_service_t* peer_memkv_get_service(void);

#endif /* PEER_MEMKV_H_ */