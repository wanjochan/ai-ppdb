#ifndef POLY_POLL_COMPAT_H
#define POLY_POLL_COMPAT_H

#include "internal/poly/poly_poll.h"
#include "internal/poly/poly_poll_async.h"

// 兼容层配置转换函数
static inline poly_poll_config_t poly_poll_create_async_config(
    const poly_poll_config_t* old_config) {
    poly_poll_config_t new_config = {
        .min_threads = 4,     // 默认4个线程
        .max_threads = 8,     // 最大8个线程
        .queue_size = 1000,   // 任务队列大小
        .max_listeners = old_config->max_listeners,
        .read_buffer_size = old_config->read_buffer_size
    };
    return new_config;
}

// 兼容层初始化函数
static inline infra_error_t poly_poll_init_compat(
    poly_poll_context_t* ctx,
    const poly_poll_config_t* old_config) {
    poly_poll_config_t new_config = poly_poll_create_async_config(old_config);
    return poly_poll_init(ctx, &new_config);
}

// 使用宏进行条件编译
#ifdef USE_ASYNC_POLL
    #define poly_poll_init poly_poll_init_compat
#endif

#endif /* POLY_POLL_COMPAT_H */
