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

// 多路复用事件结构
typedef struct {
    int fd;                     // 文件描述符
    infra_event_type_t events;  // 触发的事件
    void* user_data;           // 用户数据
} infra_mux_event_t;

// 多路复用实例（抽象接口）
typedef struct infra_mux infra_mux_t;

// 多路复用操作接口
typedef struct {
    // 销毁实例
    infra_error_t (*destroy)(infra_mux_t* mux);
    
    // 添加文件描述符
    infra_error_t (*add)(infra_mux_t* mux, int fd, infra_event_type_t events, void* user_data);
    
    // 移除文件描述符
    infra_error_t (*remove)(infra_mux_t* mux, int fd);
    
    // 修改事件
    infra_error_t (*modify)(infra_mux_t* mux, int fd, infra_event_type_t events);
    
    // 等待事件
    infra_error_t (*wait)(infra_mux_t* mux, infra_mux_event_t* events, size_t max_events, int timeout_ms);
} infra_mux_ops_t;

// 多路复用实例结构
struct infra_mux {
    const infra_mux_ops_t* ops;  // 操作接口
    void* impl;                  // 具体实现的私有数据
};

// 创建多路复用实例
infra_error_t infra_mux_create(const infra_config_t* config, infra_mux_t** mux);

// 通用操作接口
static inline infra_error_t infra_mux_destroy(infra_mux_t* mux) {
    return mux->ops->destroy(mux);
}

static inline infra_error_t infra_mux_add(infra_mux_t* mux, int fd, infra_event_type_t events, void* user_data) {
    return mux->ops->add(mux, fd, events, user_data);
}

static inline infra_error_t infra_mux_remove(infra_mux_t* mux, int fd) {
    return mux->ops->remove(mux, fd);
}

static inline infra_error_t infra_mux_modify(infra_mux_t* mux, int fd, infra_event_type_t events) {
    return mux->ops->modify(mux, fd, events);
}

static inline infra_error_t infra_mux_wait(infra_mux_t* mux, infra_mux_event_t* events, size_t max_events, int timeout_ms) {
    return mux->ops->wait(mux, events, max_events, timeout_ms);
}

#endif /* INFRA_MUX_H */ 