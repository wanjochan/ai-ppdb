#ifndef POLY_MUX_H
#define POLY_MUX_H

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_net.h"

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// 连接类型
typedef enum {
    POLY_MUX_CONN_UNKNOWN = 0,  // 未知类型
    POLY_MUX_CONN_ACCEPT,       // 接受的连接
    POLY_MUX_CONN_CONNECT       // 主动连接
} poly_mux_conn_type_t;

// 连接状态
typedef enum {
    POLY_MUX_CONN_STATE_NONE = 0,    // 初始状态
    POLY_MUX_CONN_STATE_CONNECTING,   // 正在连接
    POLY_MUX_CONN_STATE_CONNECTED,    // 已连接
    POLY_MUX_CONN_STATE_CLOSING,      // 正在关闭
    POLY_MUX_CONN_STATE_CLOSED       // 已关闭
} poly_mux_conn_state_t;

// 连接配置
typedef struct {
    size_t read_buffer_size;     // 读缓冲区大小
    size_t write_buffer_size;    // 写缓冲区大小
    int idle_timeout;            // 空闲超时(毫秒)
    int connect_timeout;         // 连接超时(毫秒)
    bool nonblocking;            // 是否非阻塞
} poly_mux_conn_config_t;

// 连接对象
typedef struct poly_mux_conn {
    poly_mux_conn_type_t type;           // 连接类型
    poly_mux_conn_state_t state;         // 连接状态
    infra_socket_t sock;                 // 套接字
    void* user_data;                     // 用户数据
    struct poly_mux* mux;                // 所属复用器
    uint64_t last_active;                // 最后活动时间
    char* read_buffer;                   // 读缓冲区
    size_t read_pos;                     // 读位置
    char* write_buffer;                  // 写缓冲区
    size_t write_pos;                    // 写位置
    struct poly_mux_conn* next;          // 下一个连接
} poly_mux_conn_t;

// 事件回调
typedef struct {
    void (*on_accept)(void* ctx, poly_mux_conn_t* listener, infra_net_addr_t* addr);  // 新连接到达
    void (*on_connect)(void* ctx, poly_mux_conn_t* conn, infra_error_t err);          // 连接完成
    void (*on_data)(void* ctx, poly_mux_conn_t* conn);                                // 数据就绪
    void (*on_writable)(void* ctx, poly_mux_conn_t* conn);                            // 可写就绪
    void (*on_error)(void* ctx, poly_mux_conn_t* conn, infra_error_t err);            // 错误发生
    void (*on_close)(void* ctx, poly_mux_conn_t* conn);                               // 连接关闭
} poly_mux_events_t;

// 复用器配置
typedef struct {
    const char* host;            // 监听地址
    int port;                    // 监听端口
    int min_threads;            // 最小线程数
    int max_threads;            // 最大线程数
    int queue_size;            // 队列大小
    int idle_timeout;          // 空闲超时(秒)
    poly_mux_conn_config_t conn_config;  // 连接配置
} poly_mux_config_t;

// 复用器对象
typedef struct poly_mux poly_mux_t;

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

// 创建复用器
infra_error_t poly_mux_create(const poly_mux_config_t* config, poly_mux_t** mux);

// 销毁复用器
void poly_mux_destroy(poly_mux_t* mux);

// 启动服务
infra_error_t poly_mux_start(poly_mux_t* mux, const poly_mux_events_t* events, void* ctx);

// 停止服务
void poly_mux_stop(poly_mux_t* mux);

// 创建监听器
infra_error_t poly_mux_listen(poly_mux_t* mux, const char* host, int port, poly_mux_conn_t** conn);

// 创建连接
infra_error_t poly_mux_connect(poly_mux_t* mux, const char* host, int port, poly_mux_conn_t** conn);

// 关闭连接
void poly_mux_conn_close(poly_mux_conn_t* conn);

// 读取数据
ssize_t poly_mux_conn_read(poly_mux_conn_t* conn, void* buf, size_t size);

// 写入数据
ssize_t poly_mux_conn_write(poly_mux_conn_t* conn, const void* data, size_t size);

// 获取连接状态
poly_mux_conn_state_t poly_mux_conn_get_state(poly_mux_conn_t* conn);

// 获取连接类型
poly_mux_conn_type_t poly_mux_conn_get_type(poly_mux_conn_t* conn);

// 获取用户数据
void* poly_mux_conn_get_user_data(poly_mux_conn_t* conn);

// 设置用户数据
void poly_mux_conn_set_user_data(poly_mux_conn_t* conn, void* data);

// 获取统计信息
infra_error_t poly_mux_get_stats(poly_mux_t* mux, uint32_t* curr_conns, uint32_t* total_conns);

#endif // POLY_MUX_H 