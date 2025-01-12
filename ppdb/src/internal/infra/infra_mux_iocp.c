/*
 * infra_mux_iocp.c - IOCP Multiplexing Implementation
 */

#include "infra_mux_iocp.h"
#include "infra_memory.h"
#include "infra_platform.h"

// IOCP实现的私有数据
typedef struct {
    void* iocp;                // IOCP句柄
    size_t max_events;         // 最大事件数
} infra_mux_iocp_impl_t;

// 销毁IOCP实例
static infra_error_t infra_mux_iocp_destroy_impl(infra_mux_t* mux) {
    if (!mux || !mux->impl) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_mux_iocp_impl_t* impl = (infra_mux_iocp_impl_t*)mux->impl;
    
    if (impl->iocp) {
        infra_platform_close_iocp(impl->iocp);
    }
    
    infra_free(impl);
    infra_free(mux);
    
    return INFRA_OK;
}

// 添加文件描述符到IOCP
static infra_error_t infra_mux_iocp_add_impl(infra_mux_t* mux, int fd, infra_event_type_t events, void* user_data) {
    if (!mux || !mux->impl || fd < 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_mux_iocp_impl_t* impl = (infra_mux_iocp_impl_t*)mux->impl;
    return infra_platform_iocp_add(impl->iocp, fd, user_data);
}

// 从IOCP移除文件描述符（IOCP不需要显式移除）
static infra_error_t infra_mux_iocp_remove_impl(infra_mux_t* mux, int fd) {
    (void)mux;
    (void)fd;
    return INFRA_OK;
}

// 修改IOCP中的事件（IOCP不需要修改事件）
static infra_error_t infra_mux_iocp_modify_impl(infra_mux_t* mux, int fd, infra_event_type_t events) {
    (void)mux;
    (void)fd;
    (void)events;
    return INFRA_OK;
}

// 等待IOCP事件
static infra_error_t infra_mux_iocp_wait_impl(infra_mux_t* mux, infra_mux_event_t* events, size_t max_events, int timeout_ms) {
    if (!mux || !mux->impl || !events || max_events == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_mux_iocp_impl_t* impl = (infra_mux_iocp_impl_t*)mux->impl;
    size_t num_events = max_events < impl->max_events ? max_events : impl->max_events;
    
    return infra_platform_iocp_wait(impl->iocp, events, num_events, timeout_ms);
}

// IOCP操作接口
static const infra_mux_ops_t iocp_ops = {
    .destroy = infra_mux_iocp_destroy_impl,
    .add = infra_mux_iocp_add_impl,
    .remove = infra_mux_iocp_remove_impl,
    .modify = infra_mux_iocp_modify_impl,
    .wait = infra_mux_iocp_wait_impl
};

// 创建IOCP实例
infra_error_t infra_mux_iocp_create(const infra_config_t* config, infra_mux_t** mux) {
    if (!config || !mux) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 分配mux结构
    infra_mux_t* new_mux = (infra_mux_t*)infra_malloc(sizeof(infra_mux_t));
    if (!new_mux) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // 分配实现结构
    infra_mux_iocp_impl_t* impl = (infra_mux_iocp_impl_t*)infra_malloc(sizeof(infra_mux_iocp_impl_t));
    if (!impl) {
        infra_free(new_mux);
        return INFRA_ERROR_NO_MEMORY;
    }

    // 创建IOCP实例
    impl->iocp = infra_platform_create_iocp();
    if (!impl->iocp) {
        infra_free(impl);
        infra_free(new_mux);
        return INFRA_ERROR_SYSTEM;
    }

    impl->max_events = config->mux.max_events;

    new_mux->ops = &iocp_ops;
    new_mux->impl = impl;

    *mux = new_mux;
    return INFRA_OK;
} 