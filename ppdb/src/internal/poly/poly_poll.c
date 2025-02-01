#include "internal/poly/poly_poll.h"
// #include "internal/infra/infra_memory.h"
// #include "internal/infra/infra_net.h"
// #include "internal/infra/infra_sync.h"
// #include "internal/infra/infra_core.h"

// 初始化 poly_poll
infra_error_t poly_poll_init(poly_poll_context_t* ctx, const poly_poll_config_t* config) {
    if (!ctx || !config) {
        INFRA_LOG_ERROR("Invalid parameters: ctx=%p, config=%p", ctx, config);
        return INFRA_ERROR_INVALID_PARAM;
    }
    if (config->max_listeners <= 0) {
        INFRA_LOG_ERROR("Invalid max_listeners: %d", config->max_listeners);
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 初始化上下文
    memset(ctx, 0, sizeof(poly_poll_context_t));
    ctx->max_listeners = config->max_listeners;

    INFRA_LOG_INFO("Initializing poly_poll with max_listeners=%d, threads=%d-%d", 
        config->max_listeners, config->min_threads, config->max_threads);

    // 创建线程池
    infra_thread_pool_config_t pool_config = {
        .min_threads = config->min_threads,
        .max_threads = config->max_threads,
        .queue_size = config->queue_size
    };

    infra_error_t err = infra_thread_pool_create(&pool_config, &ctx->pool);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create thread pool: %d", err);
        return err;
    }

    // 分配监听器数组
    ctx->listeners = malloc(config->max_listeners * sizeof(infra_socket_t));
    ctx->listener_configs = malloc(config->max_listeners * sizeof(poly_poll_listener_t));
    ctx->polls = malloc(config->max_listeners * sizeof(struct pollfd));
    
    if (!ctx->listeners || !ctx->listener_configs || !ctx->polls) {
        INFRA_LOG_ERROR("Failed to allocate memory for listeners");
        poly_poll_cleanup(ctx);
        return INFRA_ERROR_NO_MEMORY;
    }

    INFRA_LOG_INFO("Successfully initialized poly_poll");
    return INFRA_OK;
}

// 添加监听器
infra_error_t poly_poll_add_listener(poly_poll_context_t* ctx, const poly_poll_listener_t* listener) {
    if (!ctx || !listener) {
        INFRA_LOG_ERROR("Invalid parameters: ctx=%p, listener=%p", ctx, listener);
        return INFRA_ERROR_INVALID_PARAM;
    }
    if (ctx->listener_count >= ctx->max_listeners) {
        INFRA_LOG_ERROR("Exceeded max listeners: %d", ctx->max_listeners);
        return INFRA_ERROR_NO_SPACE;
    }

    INFRA_LOG_INFO("Adding listener on %s:%d", listener->bind_addr, listener->bind_port);

    // 创建监听socket
    infra_socket_t sock = NULL;
    infra_config_t config = {0};
    infra_error_t err = infra_net_create(&sock, false, &config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create listener socket: %d", err);
        return err;
    }

    // 设置地址重用
    err = infra_net_set_reuseaddr(sock, true);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set reuseaddr: %d", err);
        infra_net_close(sock);
        return err;
    }

    // 绑定地址
    infra_net_addr_t addr = {
        .host = listener->bind_addr,
        .port = listener->bind_port
    };
    err = infra_net_bind(sock, &addr);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to bind address %s:%d: %d", 
            listener->bind_addr, listener->bind_port, err);
        infra_net_close(sock);
        return err;
    }

    // 开始监听
    err = infra_net_listen(sock);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to listen on %s:%d: %d", 
            listener->bind_addr, listener->bind_port, err);
        infra_net_close(sock);
        return err;
    }

    // 设置非阻塞模式
    err = infra_net_set_nonblock(sock, true);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set nonblock: %d", err);
        infra_net_close(sock);
        return err;
    }

    // 添加到 poll 数组
    int idx = ctx->listener_count;
    ctx->polls[idx].fd = infra_net_get_fd(sock);
    ctx->polls[idx].events = POLLIN;
    ctx->polls[idx].revents = 0;

    // 保存监听器配置
    ctx->listeners[idx] = sock;
    // 分开复制字符串和端口号
    strncpy(ctx->listener_configs[idx].bind_addr, listener->bind_addr, POLY_MAX_ADDR_LEN - 1);
    ctx->listener_configs[idx].bind_addr[POLY_MAX_ADDR_LEN - 1] = '\0';
    ctx->listener_configs[idx].bind_port = listener->bind_port;
    // 直接赋值 user_data
    ctx->listener_configs[idx].user_data = listener->user_data;
    ctx->listener_count++;

    INFRA_LOG_INFO("Successfully added listener on %s:%d", listener->bind_addr, listener->bind_port);
    return INFRA_OK;
}

// 设置连接处理回调
void poly_poll_set_handler(poly_poll_context_t* ctx, poly_poll_connection_handler handler) {
    if (!ctx) {
        INFRA_LOG_ERROR("Invalid context");
        return;
    }
    ctx->handler = handler;
    INFRA_LOG_INFO("Connection handler set: %p", handler);
}

// 启动服务
infra_error_t poly_poll_start(poly_poll_context_t* ctx) {
    if (!ctx || !ctx->handler) {
        INFRA_LOG_ERROR("Invalid parameters: ctx=%p, handler=%p", ctx, ctx ? ctx->handler : NULL);
        return INFRA_ERROR_INVALID_PARAM;
    }
    if (ctx->running) {
        INFRA_LOG_ERROR("Service already running");
        return INFRA_ERROR_ALREADY_EXISTS;
    }
    if (ctx->listener_count == 0) {
        INFRA_LOG_ERROR("No listeners added");
        return INFRA_ERROR_INVALID_STATE;
    }

    INFRA_LOG_INFO("Starting poly_poll service with %d listeners", ctx->listener_count);
    ctx->running = true;

    // 主循环
    while (ctx->running) {
        // 等待事件
        int ret = poll(ctx->polls, ctx->listener_count, 1000);  // 1秒超时
        if (ret < 0) {
            if (errno == EINTR) {
                if (!ctx->running) break;
                continue;
            }
            INFRA_LOG_ERROR("Poll failed: %d", errno);
            continue;
        }
        if (ret == 0) {  // 超时
            if (!ctx->running) break;
            continue;
        }

        // 检查每个监听器
        for (int i = 0; i < ctx->listener_count; i++) {
            if (!(ctx->polls[i].revents & POLLIN)) continue;

            // 接受连接
            infra_socket_t client = NULL;
            infra_net_addr_t client_addr = {0};
            infra_error_t err = infra_net_accept(ctx->listeners[i], &client, &client_addr);
            if (err != INFRA_OK) {
                if (err == INFRA_ERROR_WOULD_BLOCK) continue;
                INFRA_LOG_ERROR("Failed to accept connection: %d", err);
                continue;
            }

            INFRA_LOG_INFO("Accepted connection from %s:%d for listener %d", 
                client_addr.host, client_addr.port, i);

            // 提交到线程池
            poly_poll_handler_args_t* args = malloc(sizeof(poly_poll_handler_args_t));
            if (!args) {
                INFRA_LOG_ERROR("Failed to allocate handler args");
                infra_net_close(client);
                continue;
            }
            args->client = client;
            args->user_data = ctx->listener_configs[i].user_data;

            err = infra_thread_pool_submit(ctx->pool, (infra_thread_func_t)ctx->handler, args);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to submit connection to thread pool: %d", err);
                free(args);
                infra_net_close(client);
                continue;
            }
        }
    }

    INFRA_LOG_INFO("Poly_poll service stopped");
    return INFRA_OK;
}

// 停止服务
infra_error_t poly_poll_stop(poly_poll_context_t* ctx) {
    if (!ctx) {
        INFRA_LOG_ERROR("Invalid context");
        return INFRA_ERROR_INVALID_PARAM;
    }

    INFRA_LOG_INFO("Stopping poly_poll service");
    ctx->running = false;
    return INFRA_OK;
}

// 清理资源
void poly_poll_cleanup(poly_poll_context_t* ctx) {
    if (!ctx) {
        INFRA_LOG_ERROR("Invalid context");
        return;
    }

    INFRA_LOG_INFO("Cleaning up poly_poll resources");

    // 停止服务
    ctx->running = false;

    // 清理监听器
    if (ctx->listeners) {
        for (int i = 0; i < ctx->listener_count; i++) {
            if (ctx->listeners[i]) {
                infra_net_close(ctx->listeners[i]);
                ctx->listeners[i] = NULL;
            }
        }
        free(ctx->listeners);
        ctx->listeners = NULL;
    }

    // 清理监听器配置数组
    if (ctx->listener_configs) {
        free(ctx->listener_configs);
        ctx->listener_configs = NULL;
    }

    // 清理 poll 数组
    if (ctx->polls) {
        free(ctx->polls);
        ctx->polls = NULL;
    }

    // 清理线程池
    if (ctx->pool) {
        infra_thread_pool_destroy(ctx->pool);
        ctx->pool = NULL;
    }

    ctx->listener_count = 0;
    ctx->max_listeners = 0;
    ctx->handler = NULL;

    INFRA_LOG_INFO("Poly_poll cleanup completed");
}
