/*
 * infra_mux.c - Multiplexing Infrastructure Layer Implementation
 */

#include "infra_mux.h"
#include "infra_core.h"

// 内部状态
struct infra_mux_ctx {
    infra_mux_type_t type;
    size_t max_events;
    bool edge_trigger;
    
    union {
#ifdef _WIN32
        struct {
            HANDLE iocp;
        } win;
#else
        struct {
            int epoll_fd;
            struct epoll_event* events;
        } nix;
#endif
        struct {
            fd_set read_fds;
            fd_set write_fds;
            fd_set error_fds;
            int max_fd;
        } select;
    } impl;
};

// 创建多路复用上下文
infra_error_t infra_mux_create(const infra_mux_config_t* config, infra_mux_ctx_t** ctx) {
    if (!config || !ctx) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    *ctx = (infra_mux_ctx_t*)infra_malloc(sizeof(infra_mux_ctx_t));
    if (!*ctx) {
        return INFRA_ERROR_NO_MEMORY;
    }

    infra_mux_ctx_t* mux = *ctx;
    mux->type = config->type;
    mux->max_events = config->max_events;
    mux->edge_trigger = config->edge_trigger;

    // 根据类型初始化具体实现
    switch (mux->type) {
#ifdef _WIN32
        case INFRA_MUX_IOCP:
        case INFRA_MUX_AUTO:
            mux->impl.win.iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
            if (!mux->impl.win.iocp) {
                infra_free(mux);
                return INFRA_ERROR_SYSTEM;
            }
            break;
#else
        case INFRA_MUX_EPOLL:
        case INFRA_MUX_AUTO:
            mux->impl.nix.epoll_fd = epoll_create1(0);
            if (mux->impl.nix.epoll_fd < 0) {
                infra_free(mux);
                return INFRA_ERROR_SYSTEM;
            }
            mux->impl.nix.events = infra_malloc(sizeof(struct epoll_event) * mux->max_events);
            if (!mux->impl.nix.events) {
                close(mux->impl.nix.epoll_fd);
                infra_free(mux);
                return INFRA_ERROR_NO_MEMORY;
            }
            break;
#endif
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

    return INFRA_OK;
}

// 销毁多路复用上下文
infra_error_t infra_mux_destroy(infra_mux_ctx_t* ctx) {
    if (!ctx) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    switch (ctx->type) {
#ifdef _WIN32
        case INFRA_MUX_IOCP:
        case INFRA_MUX_AUTO:
            CloseHandle(ctx->impl.win.iocp);
            break;
#else
        case INFRA_MUX_EPOLL:
        case INFRA_MUX_AUTO:
            close(ctx->impl.nix.epoll_fd);
            infra_free(ctx->impl.nix.events);
            break;
#endif
        case INFRA_MUX_SELECT:
            // select模式不需要特殊清理
            break;
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
#ifdef _WIN32
        case INFRA_MUX_IOCP:
        case INFRA_MUX_AUTO:
            if (!CreateIoCompletionPort((HANDLE)fd, ctx->impl.win.iocp, (ULONG_PTR)user_data, 0)) {
                return INFRA_ERROR_SYSTEM;
            }
            break;
#else
        case INFRA_MUX_EPOLL:
        case INFRA_MUX_AUTO: {
            struct epoll_event ev = {0};
            ev.events = ((events & INFRA_EVENT_READ) ? EPOLLIN : 0) |
                       ((events & INFRA_EVENT_WRITE) ? EPOLLOUT : 0) |
                       (ctx->edge_trigger ? EPOLLET : 0);
            ev.data.ptr = user_data;
            if (epoll_ctl(ctx->impl.nix.epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
                return INFRA_ERROR_SYSTEM;
            }
            break;
        }
#endif
        case INFRA_MUX_SELECT:
            if (events & INFRA_EVENT_READ) {
                FD_SET(fd, &ctx->impl.select.read_fds);
            }
            if (events & INFRA_EVENT_WRITE) {
                FD_SET(fd, &ctx->impl.select.write_fds);
            }
            FD_SET(fd, &ctx->impl.select.error_fds);
            if (fd > ctx->impl.select.max_fd) {
                ctx->impl.select.max_fd = fd;
            }
            break;
    }

    return INFRA_OK;
}

// 移除文件描述符
infra_error_t infra_mux_remove(infra_mux_ctx_t* ctx, int fd) {
    if (!ctx || fd < 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    switch (ctx->type) {
#ifdef _WIN32
        case INFRA_MUX_IOCP:
        case INFRA_MUX_AUTO:
            // IOCP不需要显式移除
            break;
#else
        case INFRA_MUX_EPOLL:
        case INFRA_MUX_AUTO:
            if (epoll_ctl(ctx->impl.nix.epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
                return INFRA_ERROR_SYSTEM;
            }
            break;
#endif
        case INFRA_MUX_SELECT:
            FD_CLR(fd, &ctx->impl.select.read_fds);
            FD_CLR(fd, &ctx->impl.select.write_fds);
            FD_CLR(fd, &ctx->impl.select.error_fds);
            if (fd == ctx->impl.select.max_fd) {
                // 重新计算max_fd
                ctx->impl.select.max_fd = -1;
                for (int i = 0; i < fd; i++) {
                    if (FD_ISSET(i, &ctx->impl.select.read_fds) ||
                        FD_ISSET(i, &ctx->impl.select.write_fds)) {
                        ctx->impl.select.max_fd = i;
                    }
                }
            }
            break;
    }

    return INFRA_OK;
}

// 修改事件
infra_error_t infra_mux_modify(infra_mux_ctx_t* ctx, int fd, infra_event_type_t events) {
    if (!ctx || fd < 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    switch (ctx->type) {
#ifdef _WIN32
        case INFRA_MUX_IOCP:
        case INFRA_MUX_AUTO:
            // IOCP不支持修改事件
            return INFRA_ERROR_NOT_SUPPORTED;
#else
        case INFRA_MUX_EPOLL:
        case INFRA_MUX_AUTO: {
            struct epoll_event ev = {0};
            ev.events = ((events & INFRA_EVENT_READ) ? EPOLLIN : 0) |
                       ((events & INFRA_EVENT_WRITE) ? EPOLLOUT : 0) |
                       (ctx->edge_trigger ? EPOLLET : 0);
            if (epoll_ctl(ctx->impl.nix.epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0) {
                return INFRA_ERROR_SYSTEM;
            }
            break;
        }
#endif
        case INFRA_MUX_SELECT:
            if (events & INFRA_EVENT_READ) {
                FD_SET(fd, &ctx->impl.select.read_fds);
            } else {
                FD_CLR(fd, &ctx->impl.select.read_fds);
            }
            if (events & INFRA_EVENT_WRITE) {
                FD_SET(fd, &ctx->impl.select.write_fds);
            } else {
                FD_CLR(fd, &ctx->impl.select.write_fds);
            }
            break;
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
#ifdef _WIN32
        case INFRA_MUX_IOCP:
        case INFRA_MUX_AUTO: {
            DWORD bytes;
            ULONG_PTR key;
            OVERLAPPED* overlapped;
            DWORD wait_time = (timeout_ms < 0) ? INFINITE : timeout_ms;

            if (GetQueuedCompletionStatus(ctx->impl.win.iocp, &bytes, &key, &overlapped, wait_time)) {
                events[0].fd = (int)key;
                events[0].events = INFRA_EVENT_READ | INFRA_EVENT_WRITE;
                events[0].user_data = (void*)key;
                num_events = 1;
            }
            break;
        }
#else
        case INFRA_MUX_EPOLL:
        case INFRA_MUX_AUTO: {
            num_events = epoll_wait(ctx->impl.nix.epoll_fd, ctx->impl.nix.events, 
                                  (int)max_events, timeout_ms);
            if (num_events < 0) {
                return INFRA_ERROR_SYSTEM;
            }
            for (int i = 0; i < num_events; i++) {
                events[i].fd = ctx->impl.nix.events[i].data.fd;
                events[i].events = 
                    ((ctx->impl.nix.events[i].events & EPOLLIN) ? INFRA_EVENT_READ : 0) |
                    ((ctx->impl.nix.events[i].events & EPOLLOUT) ? INFRA_EVENT_WRITE : 0) |
                    ((ctx->impl.nix.events[i].events & EPOLLERR) ? INFRA_EVENT_ERROR : 0);
                events[i].user_data = ctx->impl.nix.events[i].data.ptr;
            }
            break;
        }
#endif
        case INFRA_MUX_SELECT: {
            fd_set read_fds = ctx->impl.select.read_fds;
            fd_set write_fds = ctx->impl.select.write_fds;
            fd_set error_fds = ctx->impl.select.error_fds;

            struct timeval tv = {
                .tv_sec = timeout_ms / 1000,
                .tv_usec = (timeout_ms % 1000) * 1000
            };

            num_events = select(ctx->impl.select.max_fd + 1,
                              &read_fds, &write_fds, &error_fds,
                              (timeout_ms < 0) ? NULL : &tv);
            if (num_events < 0) {
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