#ifndef POLY_MUX_H
#define POLY_MUX_H

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_net.h"

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// 前向声明
typedef struct poly_mux poly_mux_t;

// 连接处理回调函数类型
typedef infra_error_t (*poly_mux_handler_t)(void* ctx, infra_socket_t sock);

// 多路复用器配置
typedef struct poly_mux_config {
    uint16_t port;             // 监听端口
    const char* host;          // 监听地址
    size_t max_connections;    // 最大连接数
    size_t min_threads;        // 最小线程数
    size_t max_threads;        // 最大线程数
    size_t queue_size;         // 任务队列大小
    uint32_t idle_timeout;     // 空闲超时(秒)
} poly_mux_config_t;

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

/**
 * @brief 创建多路复用器
 * @param config 配置信息
 * @param mux 输出参数，返回创建的多路复用器
 * @return 错误码
 */
infra_error_t poly_mux_create(const poly_mux_config_t* config, poly_mux_t** mux);

/**
 * @brief 销毁多路复用器
 * @param mux 多路复用器
 */
void poly_mux_destroy(poly_mux_t* mux);

/**
 * @brief 启动多路复用器
 * @param mux 多路复用器
 * @param handler 连接处理回调
 * @param ctx 回调上下文
 * @return 错误码
 */
infra_error_t poly_mux_start(poly_mux_t* mux, poly_mux_handler_t handler, void* ctx);

/**
 * @brief 停止多路复用器
 * @param mux 多路复用器
 * @return 错误码
 */
infra_error_t poly_mux_stop(poly_mux_t* mux);

/**
 * @brief 获取统计信息
 * @param mux 多路复用器
 * @param curr_conns 当前连接数
 * @param total_conns 总连接数
 * @return 错误码
 */
infra_error_t poly_mux_get_stats(poly_mux_t* mux, size_t* curr_conns, size_t* total_conns);

/**
 * @brief 检查多路复用器是否正在运行
 * @param mux 多路复用器
 * @return true 如果正在运行
 */
bool poly_mux_is_running(const poly_mux_t* mux);

#endif // POLY_MUX_H 