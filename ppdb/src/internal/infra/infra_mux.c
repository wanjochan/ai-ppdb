/*
 * infra_mux.c - Multiplexing Infrastructure Layer Implementation
 */

#include "infra_mux.h"
#include "infra_core.h"
#include "infra_error.h"
#include "infra_memory.h"
#include "infra_platform.h"

// 内部状态
struct infra_mux_ctx {
    infra_mux_type_t type;
    size_t max_events;
    bool edge_trigger;
    
    union {
        struct {
            void* iocp;  // IOCP句柄
        } win;
        struct {
            int epoll_fd;
            void* events;
        } nix;
        struct {
            int max_fd;
            fd_set read_fds;
            fd_set write_fds;
            fd_set error_fds;
        } select;
    } impl;
};

// 创建多路复用上下文
infra_error_t infra_mux_create(const infra_mux_config_t* config, infra_mux_ctx_t** ctx) {
    if (!config || !ctx) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_mux_ctx_t* mux = (infra_mux_ctx_t*)infra_malloc(sizeof(infra_mux_ctx_t));
    if (!mux) {
        return INFRA_ERROR_NO_MEMORY;
    }

    memset(mux, 0, sizeof(infra_mux_ctx_t));
    mux->type = config->type;
    mux->max_events = config->max_events;
    mux->edge_trigger = config->edge_trigger;

    switch (config->type) {
        case INFRA_MUX_AUTO:
            // 根据平台选择最优的多路复用方式
            if (infra_platform_is_windows()) {
                mux->type = INFRA_MUX_IOCP;
            } else {
                mux->type = INFRA_MUX_EPOLL;
            }
            // 继续执行对应类型的初始化
            // fallthrough
            
        case INFRA_MUX_IOCP:
            if (infra_platform_is_windows()) {
                mux->impl.win.iocp = infra_platform_create_iocp();
                if (!mux->impl.win.iocp) {
                    infra_free(mux);
                    return INFRA_ERROR_SYSTEM;
                }
            } else {
                infra_free(mux);
                return INFRA_ERROR_NOT_SUPPORTED;
            }
            break;
            
        case INFRA_MUX_EPOLL:
            if (!infra_platform_is_windows()) {
                mux->impl.nix.epoll_fd = infra_platform_create_epoll();
                if (mux->impl.nix.epoll_fd < 0) {
                    infra_free(mux);
                    return INFRA_ERROR_SYSTEM;
                }
            } else {
                infra_free(mux);
                return INFRA_ERROR_NOT_SUPPORTED;
            }
            break;
            
        case INFRA_MUX_SELECT:
            FD_ZERO(&mux->impl.select.read_fds);
            FD_ZERO(&mux->impl.select.write_fds);
            FD_ZERO(&mux->impl.select.error_fds);
            mux->impl.select.max_fd = -1;
            break;
            
        default:
            infra_free(mux);
            return INFRA_ERROR_INVALID_PARAM;
    }

    *ctx = mux;
    return INFRA_OK;
}

// 销毁多路复用上下文
infra_error_t infra_mux_destroy(infra_mux_ctx_t* ctx) {
    if (!ctx) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    switch (ctx->type) {
        case INFRA_MUX_AUTO:
        case INFRA_MUX_IOCP:
            if (infra_platform_is_windows()) {
                if (ctx->impl.win.iocp) {
                    infra_platform_close_iocp(ctx->impl.win.iocp);
                }
            }
            break;
            
        case INFRA_MUX_EPOLL:
            if (!infra_platform_is_windows()) {
                if (ctx->impl.nix.epoll_fd >= 0) {
                    infra_platform_close_epoll(ctx->impl.nix.epoll_fd);
                }
            }
            break;
            
        case INFRA_MUX_SELECT:
            // select模式不需要特殊清理
            break;
            
        default:
            return INFRA_ERROR_INVALID_PARAM;
    }

    infra_free(ctx);
    return INFRA_OK;
}

// 添加文件描述符
infra_error_t infra_mux_add(infra_mux_ctx_t* ctx, int fd, infra_event_type_t events, void* user_data) {
    if (!ctx || fd < 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    switch (ctx->type) {
        case INFRA_MUX_AUTO:
        case INFRA_MUX_IOCP:
            if (infra_platform_is_windows()) {
                infra_error_t err = infra_platform_iocp_add(ctx->impl.win.iocp, fd, user_data);
                if (err == INFRA_ERROR_ALREADY_EXISTS) {
                    // 已经关联过了，忽略错误
                    break;
                }
                return err;
            }
            return INFRA_ERROR_NOT_SUPPORTED;
            
        case INFRA_MUX_EPOLL:
            if (!infra_platform_is_windows()) {
                return infra_platform_epoll_add(ctx->impl.nix.epoll_fd, fd, events, 
                                              ctx->edge_trigger, user_data);
            }
            return INFRA_ERROR_NOT_SUPPORTED;
            
        case INFRA_MUX_SELECT: {
            if (fd > FD_SETSIZE) {
                return INFRA_ERROR_INVALID_PARAM;
            }

            if (events & INFRA_EVENT_READ) {
                FD_SET(fd, &ctx->impl.select.read_fds);
            }
            if (events & INFRA_EVENT_WRITE) {
                FD_SET(fd, &ctx->impl.select.write_fds);
            }
            if (events & INFRA_EVENT_ERROR) {
                FD_SET(fd, &ctx->impl.select.error_fds);
            }

            if (fd > ctx->impl.select.max_fd) {
                ctx->impl.select.max_fd = fd;
            }
            break;
        }
            
        default:
            return INFRA_ERROR_INVALID_PARAM;
    }

    return INFRA_OK;
}

// 移除文件描述符
infra_error_t infra_mux_remove(infra_mux_ctx_t* ctx, int fd) {
    if (!ctx || fd < 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    switch (ctx->type) {
        case INFRA_MUX_AUTO:
        case INFRA_MUX_IOCP:
            if (infra_platform_is_windows()) {
                // IOCP不需要显式移除
                return INFRA_OK;
            }
            return INFRA_ERROR_NOT_SUPPORTED;
            
        case INFRA_MUX_EPOLL:
            if (!infra_platform_is_windows()) {
                return infra_platform_epoll_remove(ctx->impl.nix.epoll_fd, fd);
            }
            return INFRA_ERROR_NOT_SUPPORTED;
            
        case INFRA_MUX_SELECT: {
            FD_CLR(fd, &ctx->impl.select.read_fds);
            FD_CLR(fd, &ctx->impl.select.write_fds);
            FD_CLR(fd, &ctx->impl.select.error_fds);

            // 如果删除的是最大fd，需要重新计算最大fd
            if (fd == ctx->impl.select.max_fd) {
                int max_fd = -1;
                for (int i = 0; i <= ctx->impl.select.max_fd; i++) {
                    if (FD_ISSET(i, &ctx->impl.select.read_fds) ||
                        FD_ISSET(i, &ctx->impl.select.write_fds) ||
                        FD_ISSET(i, &ctx->impl.select.error_fds)) {
                        max_fd = i;
                    }
                }
                ctx->impl.select.max_fd = max_fd;
            }
            break;
        }
            
        default:
            return INFRA_ERROR_INVALID_PARAM;
    }

    return INFRA_OK;
}

// 修改事件
infra_error_t infra_mux_modify(infra_mux_ctx_t* ctx, int fd, infra_event_type_t events) {
    if (!ctx || fd < 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    switch (ctx->type) {
        case INFRA_MUX_AUTO:
        case INFRA_MUX_IOCP:
            // IOCP不支持修改事件
            return INFRA_ERROR_NOT_SUPPORTED;
            
        case INFRA_MUX_EPOLL:
            if (!infra_platform_is_windows()) {
                return infra_platform_epoll_modify(ctx->impl.nix.epoll_fd, fd, events, 
                                                 ctx->edge_trigger);
            }
            return INFRA_ERROR_NOT_SUPPORTED;
            
        case INFRA_MUX_SELECT: {
            if (fd > FD_SETSIZE) {
                return INFRA_ERROR_INVALID_PARAM;
            }

            // 先清除所有事件
            FD_CLR(fd, &ctx->impl.select.read_fds);
            FD_CLR(fd, &ctx->impl.select.write_fds);
            FD_CLR(fd, &ctx->impl.select.error_fds);

            // 重新设置事件
            if (events & INFRA_EVENT_READ) {
                FD_SET(fd, &ctx->impl.select.read_fds);
            }
            if (events & INFRA_EVENT_WRITE) {
                FD_SET(fd, &ctx->impl.select.write_fds);
            }
            if (events & INFRA_EVENT_ERROR) {
                FD_SET(fd, &ctx->impl.select.error_fds);
            }

            // 如果没有任何事件，且是最大fd，需要重新计算最大fd
            if (!(events & (INFRA_EVENT_READ | INFRA_EVENT_WRITE | INFRA_EVENT_ERROR)) &&
                fd == ctx->impl.select.max_fd) {
                int max_fd = -1;
                for (int i = 0; i <= ctx->impl.select.max_fd; i++) {
                    if (FD_ISSET(i, &ctx->impl.select.read_fds) ||
                        FD_ISSET(i, &ctx->impl.select.write_fds) ||
                        FD_ISSET(i, &ctx->impl.select.error_fds)) {
                        max_fd = i;
                    }
                }
                ctx->impl.select.max_fd = max_fd;
            }
            break;
        }
            
        default:
            return INFRA_ERROR_INVALID_PARAM;
    }

    return INFRA_OK;
}

// 等待事件
infra_error_t infra_mux_wait(infra_mux_ctx_t* ctx, infra_mux_event_t* events, size_t max_events, int timeout_ms) {
    if (!ctx || !events || max_events == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    int num_events = 0;

    switch (ctx->type) {
        case INFRA_MUX_AUTO:
        case INFRA_MUX_IOCP:
            if (infra_platform_is_windows()) {
                return infra_platform_iocp_wait(ctx->impl.win.iocp, events, max_events, timeout_ms);
            }
            return INFRA_ERROR_NOT_SUPPORTED;
            
        case INFRA_MUX_EPOLL:
            if (!infra_platform_is_windows()) {
                return infra_platform_epoll_wait(ctx->impl.nix.epoll_fd, events, max_events, timeout_ms);
            }
            return INFRA_ERROR_NOT_SUPPORTED;
            
        case INFRA_MUX_SELECT: {
            // 复制 fd_set 结构体
            fd_set read_fds, write_fds, error_fds;
            memcpy(&read_fds, &ctx->impl.select.read_fds, sizeof(fd_set));
            memcpy(&write_fds, &ctx->impl.select.write_fds, sizeof(fd_set));
            memcpy(&error_fds, &ctx->impl.select.error_fds, sizeof(fd_set));

            struct timeval tv = {0};
            struct timeval* ptv = NULL;
            if (timeout_ms >= 0) {
                tv.tv_sec = timeout_ms / 1000;
                tv.tv_usec = (timeout_ms % 1000) * 1000;
                ptv = &tv;
            }

            num_events = select(ctx->impl.select.max_fd + 1,
                              &read_fds, &write_fds, &error_fds, ptv);
            if (num_events < 0) {
                if (errno == EINTR) {
                    return 0;  // 被信号中断，返回0个事件
                }
                return INFRA_ERROR_SYSTEM;
            }

            int count = 0;
            for (int fd = 0; fd <= ctx->impl.select.max_fd && count < (int)max_events; fd++) {
                infra_event_type_t evt = INFRA_EVENT_NONE;
                if (FD_ISSET(fd, &read_fds)) evt |= INFRA_EVENT_READ;
                if (FD_ISSET(fd, &write_fds)) evt |= INFRA_EVENT_WRITE;
                if (FD_ISSET(fd, &error_fds)) evt |= INFRA_EVENT_ERROR;

                if (evt) {
                    events[count].fd = fd;
                    events[count].events = evt;
                    events[count].user_data = NULL; // select模式不支持user_data
                    count++;
                }
            }
            num_events = count;
            break;
        }
            
        default:
            return INFRA_ERROR_INVALID_PARAM;
    }

    return num_events;
}

// 获取多路复用类型
infra_error_t infra_mux_get_type(infra_mux_ctx_t* ctx, infra_mux_type_t* type) {
    if (!ctx || !type) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    *type = ctx->type;
    return INFRA_OK;
}

// 获取多路复用类型名称
const char* infra_mux_get_type_name(infra_mux_type_t type) {
    switch (type) {
        case INFRA_MUX_AUTO: return "auto";
        case INFRA_MUX_EPOLL: return "epoll";
        case INFRA_MUX_IOCP: return "iocp";
        case INFRA_MUX_SELECT: return "select";
        default: return "unknown";
    }
} 