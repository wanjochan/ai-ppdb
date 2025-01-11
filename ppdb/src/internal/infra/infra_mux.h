/*
 * infra_mux.h - Multiplexing Infrastructure Layer Interface
 */

#ifndef INFRA_MUX_H
#define INFRA_MUX_H

#include "infra_core.h"

// 事件类型定义
typedef enum {
    INFRA_EVENT_NONE = 0,
    INFRA_EVENT_READ = 1 << 0,
    INFRA_EVENT_WRITE = 1 << 1,
    INFRA_EVENT_ERROR = 1 << 2
} infra_event_type_t;

// 多路复用类型
typedef enum {
    INFRA_MUX_AUTO,     // 自动选择(Windows:IOCP, Others:epoll)
    INFRA_MUX_EPOLL,    // 强制使用epoll
    INFRA_MUX_IOCP,     // 强制使用IOCP
    INFRA_MUX_SELECT    // 使用select(用于调试)
} infra_mux_type_t;

// 多路复用事件结构
typedef struct {
    int fd;                     // 文件描述符
    infra_event_type_t events;  // 触发的事件
    void* user_data;           // 用户数据
} infra_mux_event_t;

// 多路复用上下文
typedef struct infra_mux_ctx infra_mux_ctx_t;

// 多路复用配置
typedef struct {
    infra_mux_type_t type;      // 多路复用类型
    size_t max_events;          // 单次处理的最大事件数
    bool edge_trigger;          // 是否启用边缘触发(仅epoll)
} infra_mux_config_t;

// 多路复用接口
infra_error_t infra_mux_create(const infra_mux_config_t* config, infra_mux_ctx_t** ctx);
infra_error_t infra_mux_destroy(infra_mux_ctx_t* ctx);
infra_error_t infra_mux_add(infra_mux_ctx_t* ctx, int fd, infra_event_type_t events, void* user_data);
infra_error_t infra_mux_remove(infra_mux_ctx_t* ctx, int fd);
infra_error_t infra_mux_modify(infra_mux_ctx_t* ctx, int fd, infra_event_type_t events);
infra_error_t infra_mux_wait(infra_mux_ctx_t* ctx, infra_mux_event_t* events, size_t max_events, int timeout_ms);

// 状态查询
infra_error_t infra_mux_get_type(infra_mux_ctx_t* ctx, infra_mux_type_t* type);
const char* infra_mux_get_type_name(infra_mux_type_t type);

#endif /* INFRA_MUX_H */ 