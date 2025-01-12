/*
 * infra_mux.c - Multiplexing Infrastructure Layer Implementation
 */

#include "infra_mux.h"
#include "infra_mux_epoll.h"
#include "infra_mux_iocp.h"
#include "infra_platform.h"

// 创建多路复用实例
infra_error_t infra_mux_create(const infra_config_t* config, infra_mux_t** mux) {
    if (!config || !mux) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 根据配置选择实现
    if (config->mux.force_iocp) {
        return infra_mux_iocp_create(config, mux);
    }
    
    if (config->mux.force_epoll) {
        return infra_mux_epoll_create(config, mux);
    }
    
    // 自动选择
    if (infra_platform_is_windows() && config->mux.prefer_iocp) {
        return infra_mux_iocp_create(config, mux);
    }
    
    return infra_mux_epoll_create(config, mux);
} 