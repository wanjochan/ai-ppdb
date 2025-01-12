/*
 * infra_mux_epoll.c - EPOLL Multiplexing Implementation
 */

#include "infra_mux_epoll.h"
#include "infra_memory.h"
#include "infra_platform.h"

// epoll实现的私有数据
typedef struct {
    int epoll_fd;               // epoll文件描述符
    bool edge_trigger;          // 是否使用边缘触发
    struct epoll_event* events; // 事件缓冲区
    size_t max_events;         // 最大事件数
} infra_mux_epoll_impl_t;

// 销毁epoll实例
static infra_error_t infra_mux_epoll_destroy(infra_mux_t* mux) {
    if (!mux || !mux->impl) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_mux_epoll_impl_t* impl = (infra_mux_epoll_impl_t*)mux->impl;
    
    if (impl->events) {
        infra_free(impl->events);
    }
    
    if (impl->epoll_fd >= 0) {
        infra_platform_close_epoll(impl->epoll_fd);
    }
    
    infra_free(impl);
    infra_free(mux);
    
    return INFRA_OK;
}

// 添加文件描述符到epoll
static infra_error_t infra_mux_epoll_add(infra_mux_t* mux, int fd, infra_event_type_t events, void* user_data) {
    if (!mux || !mux->impl || fd < 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_mux_epoll_impl_t* impl = (infra_mux_epoll_impl_t*)mux->impl;
    return infra_platform_epoll_add(impl->epoll_fd, fd, events, impl->edge_trigger, user_data);
}

// 从epoll移除文件描述符
static infra_error_t infra_mux_epoll_remove(infra_mux_t* mux, int fd) {
    if (!mux || !mux->impl || fd < 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_mux_epoll_impl_t* impl = (infra_mux_epoll_impl_t*)mux->impl;
    return infra_platform_epoll_remove(impl->epoll_fd, fd);
}

// 修改epoll中的事件
static infra_error_t infra_mux_epoll_modify(infra_mux_t* mux, int fd, infra_event_type_t events) {
    if (!mux || !mux->impl || fd < 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_mux_epoll_impl_t* impl = (infra_mux_epoll_impl_t*)mux->impl;
    return infra_platform_epoll_modify(impl->epoll_fd, fd, events, impl->edge_trigger);
}

// 等待epoll事件
static infra_error_t infra_mux_epoll_wait(infra_mux_t* mux, infra_mux_event_t* events, size_t max_events, int timeout_ms) {
    if (!mux || !mux->impl || !events || max_events == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_mux_epoll_impl_t* impl = (infra_mux_epoll_impl_t*)mux->impl;
    size_t num_events = max_events < impl->max_events ? max_events : impl->max_events;
    
    // 使用内部的事件缓冲区
    infra_error_t err = infra_platform_epoll_wait(impl->epoll_fd, impl->events, num_events, timeout_ms);
    if (err < 0) {
        return err;
    }

    // 转换事件格式
    for (int i = 0; i < err; i++) {
        events[i].fd = impl->events[i].data.fd;
        events[i].events = ((impl->events[i].events & EPOLLIN) ? INFRA_EVENT_READ : 0) |
                          ((impl->events[i].events & EPOLLOUT) ? INFRA_EVENT_WRITE : 0) |
                          ((impl->events[i].events & EPOLLERR) ? INFRA_EVENT_ERROR : 0);
        events[i].user_data = impl->events[i].data.ptr;
    }

    return err;
}

// epoll操作接口
static const infra_mux_ops_t epoll_ops = {
    .destroy = infra_mux_epoll_destroy,
    .add = infra_mux_epoll_add,
    .remove = infra_mux_epoll_remove,
    .modify = infra_mux_epoll_modify,
    .wait = infra_mux_epoll_wait
};

// 创建epoll实例
infra_error_t infra_mux_epoll_create(const infra_config_t* config, infra_mux_t** mux) {
    if (!config || !mux) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 分配mux结构
    infra_mux_t* new_mux = (infra_mux_t*)infra_malloc(sizeof(infra_mux_t));
    if (!new_mux) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // 分配实现结构
    infra_mux_epoll_impl_t* impl = (infra_mux_epoll_impl_t*)infra_malloc(sizeof(infra_mux_epoll_impl_t));
    if (!impl) {
        infra_free(new_mux);
        return INFRA_ERROR_NO_MEMORY;
    }

    // 创建epoll实例
    impl->epoll_fd = infra_platform_create_epoll();
    if (impl->epoll_fd < 0) {
        infra_free(impl);
        infra_free(new_mux);
        return INFRA_ERROR_SYSTEM;
    }

    // 分配事件缓冲区
    impl->events = (struct epoll_event*)infra_malloc(sizeof(struct epoll_event) * config->mux.max_events);
    if (!impl->events) {
        infra_platform_close_epoll(impl->epoll_fd);
        infra_free(impl);
        infra_free(new_mux);
        return INFRA_ERROR_NO_MEMORY;
    }

    impl->edge_trigger = config->mux.edge_trigger;
    impl->max_events = config->mux.max_events;

    new_mux->ops = &epoll_ops;
    new_mux->impl = impl;

    *mux = new_mux;
    return INFRA_OK;
} 