/*
 * infra_mux.c - Multiplexing Infrastructure Layer Implementation
 */

#include "infra_mux.h"
#include "infra_mux_epoll.h"
#include "infra_mux_iocp.h"
#include "infra_platform.h"

infra_error_t infra_mux_create(const infra_config_t* config, infra_mux_t** mux) {
    if (!config || !mux) { return INFRA_ERROR_INVALID_PARAM; }
    
    if (infra_platform_is_windows()) {
        // Windows平台上使用IOCP，忽略edge_trigger设置
        return infra_mux_iocp_create(config, mux);
    }
    
    // 非Windows平台使用epoll
    return infra_mux_epoll_create(config, mux);
} 
