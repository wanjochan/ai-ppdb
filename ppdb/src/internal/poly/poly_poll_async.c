#include "internal/poly/poly_poll_async.h"
#include "internal/infra/infra_async.h"
#include "internal/infra/infra_log.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_thread.h"

#include <poll.h>
#include <errno.h>

// 线程池上下文
typedef struct {
    int thread_id;
    poly_poll_context_t* ctx;
} thread_context_t;

// 处理单个客户端连接的协程
static void handle_client(void* arg) {
    struct {
        infra_socket_t client;
        void* user_data;
        poly_poll_handler_fn handler;
    }* args = arg;
    
    // 调用用户处理函数
    args->handler(args->client, args->user_data);
    
    // 关闭连接
    infra_net_close(args->client);
    free(args);
}

// 线程池工作函数
static void* thread_worker(void* arg) {
    thread_context_t* ctx = arg;
    
    // 创建线程特定的协程调度器
    while (ctx->ctx->running) {
        // 运行协程调度器
        infra_async_run();
        
        // 短暂休眠避免空转
        infra_sleep_ms(1);
    }
    
    return NULL;
}

// 初始化
infra_error_t poly_poll_init(poly_poll_context_t* ctx, const poly_poll_config_t* config) {
    if (!ctx || !config) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->running = false;
    ctx->handler = NULL;
    ctx->user_data = config->user_data;
    
    // 创建线程池
    infra_thread_pool_config_t pool_config = {
        .min_threads = 4,  // 默认4个线程
        .max_threads = 8,  // 最大8个线程
        .queue_size = 1000 // 队列大小
    };
    
    infra_error_t err = infra_thread_pool_create(&pool_config, &ctx->thread_pool);
    if (err != INFRA_OK) {
        return err;
    }
    
    // 创建poll数组
    ctx->poll_size = 16;  // 初始大小
    ctx->poll_count = 0;
    ctx->poll_fds = malloc(sizeof(struct pollfd) * ctx->poll_size);
    ctx->poll_data = malloc(sizeof(void*) * ctx->poll_size);
    
    if (!ctx->poll_fds || !ctx->poll_data) {
        poly_poll_cleanup(ctx);
        return INFRA_ERROR_NO_MEMORY;
    }
    
    return INFRA_OK;
}

// 添加监听器
infra_error_t poly_poll_add_listener(poly_poll_context_t* ctx, const poly_poll_listener_t* listener) {
    if (!ctx || !listener) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    // 检查是否需要扩容
    if (ctx->poll_count >= ctx->poll_size) {
        size_t new_size = ctx->poll_size * 2;
        struct pollfd* new_fds = realloc(ctx->poll_fds, sizeof(struct pollfd) * new_size);
        void** new_data = realloc(ctx->poll_data, sizeof(void*) * new_size);
        
        if (!new_fds || !new_data) {
            if (new_fds) ctx->poll_fds = new_fds;
            if (new_data) ctx->poll_data = new_data;
            return INFRA_ERROR_NO_MEMORY;
        }
        
        ctx->poll_fds = new_fds;
        ctx->poll_data = new_data;
        ctx->poll_size = new_size;
    }
    
    // 添加到poll数组
    ctx->poll_fds[ctx->poll_count].fd = (int)(intptr_t)listener->sock;
    ctx->poll_fds[ctx->poll_count].events = POLLIN;
    ctx->poll_data[ctx->poll_count] = listener->user_data;
    ctx->poll_count++;
    
    return INFRA_OK;
}

// 设置处理函数
void poly_poll_set_handler(poly_poll_context_t* ctx, poly_poll_handler_fn handler) {
    ctx->handler = handler;
}

// 启动服务
infra_error_t poly_poll_start(poly_poll_context_t* ctx) {
    if (!ctx || !ctx->handler) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    ctx->running = true;
    
    // 创建工作线程
    for (int i = 0; i < 4; i++) {  // 创建4个工作线程
        thread_context_t* thread_ctx = malloc(sizeof(*thread_ctx));
        thread_ctx->thread_id = i;
        thread_ctx->ctx = ctx;
        
        infra_error_t err = infra_thread_pool_submit(ctx->thread_pool, thread_worker, thread_ctx);
        if (err != INFRA_OK) {
            free(thread_ctx);
            return err;
        }
    }
    
    // 主循环 - poll监听
    while (ctx->running) {
        int n = poll(ctx->poll_fds, ctx->poll_count, 100);  // 100ms超时
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        
        // 处理就绪的文件描述符
        for (size_t i = 0; i < ctx->poll_count && n > 0; i++) {
            if (ctx->poll_fds[i].revents & POLLIN) {
                n--;
                
                // 接受新连接
                infra_socket_t client;
                infra_error_t err = infra_net_accept((infra_socket_t)(intptr_t)ctx->poll_fds[i].fd, &client);
                if (err != INFRA_OK) continue;
                
                // 创建客户端处理参数
                struct {
                    infra_socket_t client;
                    void* user_data;
                    poly_poll_handler_fn handler;
                }* args = malloc(sizeof(*args));
                
                args->client = client;
                args->user_data = ctx->poll_data[i];
                args->handler = ctx->handler;
                
                // 创建协程处理连接
                infra_async_create(handle_client, args);
            }
        }
    }
    
    return INFRA_OK;
}

// 停止服务
infra_error_t poly_poll_stop(poly_poll_context_t* ctx) {
    if (!ctx) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    ctx->running = false;
    return INFRA_OK;
}

// 清理资源
void poly_poll_cleanup(poly_poll_context_t* ctx) {
    if (!ctx) {
        return;
    }
    
    ctx->running = false;
    
    // 销毁线程池
    if (ctx->thread_pool) {
        infra_thread_pool_destroy(ctx->thread_pool);
        ctx->thread_pool = NULL;
    }
    
    // 释放poll数组
    if (ctx->poll_fds) {
        free(ctx->poll_fds);
        ctx->poll_fds = NULL;
    }
    if (ctx->poll_data) {
        free(ctx->poll_data);
        ctx->poll_data = NULL;
    }
    
    ctx->poll_count = 0;
    ctx->poll_size = 0;
    ctx->handler = NULL;
    ctx->user_data = NULL;
}
