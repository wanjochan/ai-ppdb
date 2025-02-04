#include "internal/poly/poly_poll_async.h"
#include "internal/infra/infra_async.h"
#include "internal/infra/infra_log.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_thread.h"

#include <poll.h>
#include <errno.h>

// 服务状态
typedef enum {
    SERVICE_STATE_INIT = 0,
    SERVICE_STATE_RUNNING,
    SERVICE_STATE_STOPPING,
    SERVICE_STATE_STOPPED,
    SERVICE_STATE_ERROR
} service_state_t;

// 线程池上下文
typedef struct thread_context {
    int thread_id;
    poly_poll_context_t* ctx;
    bool running;
    struct thread_context* next;  // 用于线程链表
} thread_context_t;

// 线程管理器
typedef struct {
    thread_context_t* head;     // 线程链表头
    size_t count;              // 当前线程数
    infra_mutex_t mutex;       // 保护线程链表的互斥锁
} thread_manager_t;

// 处理单个客户端连接的协程
static void handle_client(void* arg) {
    struct {
        infra_socket_t client;
        void* user_data;
        poly_poll_handler_fn handler;
        poly_poll_context_t* ctx;
    }* args = arg;
    
    // 检查服务状态
    if (!args || !args->ctx || args->ctx->state != SERVICE_STATE_RUNNING) {
        if (args) {
            if (args->client) infra_net_close(args->client);
            free(args);
        }
        return;
    }
    
    // 调用用户处理函数
    args->handler(args->client, args->user_data);
    
    // 关闭连接
    infra_net_close(args->client);
    free(args);
    
    // 减少活跃协程计数
    __atomic_fetch_sub(&args->ctx->active_coroutines, 1, __ATOMIC_SEQ_CST);
}

// 添加线程到管理器
static infra_error_t add_thread(thread_manager_t* mgr, thread_context_t* thread) {
    if (!mgr || !thread) return INFRA_ERROR_INVALID_PARAM;
    
    infra_mutex_lock(&mgr->mutex);
    
    // 添加到链表头部
    thread->next = mgr->head;
    mgr->head = thread;
    mgr->count++;
    
    infra_mutex_unlock(&mgr->mutex);
    return INFRA_OK;
}

// 从管理器移除线程
static void remove_thread(thread_manager_t* mgr, thread_context_t* thread) {
    if (!mgr || !thread) return;
    
    infra_mutex_lock(&mgr->mutex);
    
    if (mgr->head == thread) {
        mgr->head = thread->next;
        mgr->count--;
    } else {
        thread_context_t* prev = mgr->head;
        while (prev && prev->next != thread) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = thread->next;
            mgr->count--;
        }
    }
    
    infra_mutex_unlock(&mgr->mutex);
}

// 线程池工作函数
static void* thread_worker(void* arg) {
    thread_context_t* ctx = arg;
    if (!ctx || !ctx->ctx) {
        INFRA_LOG_ERROR("Invalid thread context");
        return NULL;
    }
    
    ctx->running = true;
    INFRA_LOG_INFO("Worker thread %d started", ctx->thread_id);
    
    // 创建线程特定的协程调度器
    while (ctx->running && ctx->ctx->state == SERVICE_STATE_RUNNING) {
        // 运行协程调度器
        infra_async_run();
        
        // 短暂休眠避免空转
        infra_sleep_ms(1);
    }
    
    // 等待剩余协程完成
    while (ctx->ctx->active_coroutines > 0 && 
           ctx->ctx->state == SERVICE_STATE_STOPPING) {
        infra_async_run();
        infra_sleep_ms(1);
    }
    
    INFRA_LOG_INFO("Worker thread %d stopping", ctx->thread_id);
    ctx->running = false;
    
    // 从线程管理器移除自己
    remove_thread(&ctx->ctx->thread_mgr, ctx);
    free(ctx);
    return NULL;
}

// accept协程 - 在主线程中运行
static void accept_coroutine(void* arg) {
    poly_poll_context_t* ctx = arg;
    if (!ctx) {
        INFRA_LOG_ERROR("Invalid context in accept_coroutine");
        return;
    }
    
    INFRA_LOG_INFO("Accept coroutine started");
    
    while (ctx->state == SERVICE_STATE_RUNNING) {
        // poll监听
        int n = poll(ctx->poll_fds, ctx->poll_count, 100);  // 100ms超时
        if (n < 0) {
            if (errno == EINTR) {
                infra_async_yield();
                continue;
            }
            INFRA_LOG_ERROR("Poll failed: %s", strerror(errno));
            ctx->state = SERVICE_STATE_ERROR;
            break;
        }
        
        // 处理就绪的文件描述符
        for (size_t i = 0; i < ctx->poll_count && n > 0; i++) {
            if (ctx->poll_fds[i].revents & POLLIN) {
                n--;
                
                // 接受新连接
                infra_socket_t client;
                infra_error_t err = infra_net_accept(
                    (infra_socket_t)(intptr_t)ctx->poll_fds[i].fd, 
                    &client
                );
                
                if (err == INFRA_ERROR_WOULD_BLOCK) {
                    infra_async_yield();
                    continue;
                }
                if (err != INFRA_OK) {
                    INFRA_LOG_ERROR("Accept failed: %d", err);
                    continue;
                }
                
                // 创建客户端处理参数
                struct {
                    infra_socket_t client;
                    void* user_data;
                    poly_poll_handler_fn handler;
                    poly_poll_context_t* ctx;
                }* args = malloc(sizeof(*args));
                
                if (!args) {
                    INFRA_LOG_ERROR("Failed to allocate client args");
                    infra_net_close(client);
                    continue;
                }
                
                args->client = client;
                args->user_data = ctx->poll_data[i];
                args->handler = ctx->handler;
                args->ctx = ctx;
                
                // 增加活跃协程计数
                __atomic_fetch_add(&ctx->active_coroutines, 1, __ATOMIC_SEQ_CST);
                
                // 创建协程处理连接
                infra_async_create(handle_client, args);
            }
        }
        
        // 让出CPU
        infra_async_yield();
    }
    
    INFRA_LOG_INFO("Accept coroutine exiting, state: %d", ctx->state);
}

// 初始化
infra_error_t poly_poll_init(poly_poll_context_t* ctx, const poly_poll_config_t* config) {
    if (!ctx || !config) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->state = SERVICE_STATE_INIT;
    ctx->handler = NULL;
    ctx->user_data = config->user_data;
    ctx->active_coroutines = 0;
    
    // 初始化线程管理器
    ctx->thread_mgr.head = NULL;
    ctx->thread_mgr.count = 0;
    infra_error_t err = infra_mutex_create(&ctx->thread_mgr.mutex);
    if (err != INFRA_OK) {
        return err;
    }
    
    // 创建线程池
    infra_thread_pool_config_t pool_config = {
        .min_threads = 4,  // 默认4个线程
        .max_threads = 8,  // 最大8个线程
        .queue_size = 1000 // 队列大小
    };
    
    err = infra_thread_pool_create(&pool_config, &ctx->thread_pool);
    if (err != INFRA_OK) {
        infra_mutex_destroy(&ctx->thread_mgr.mutex);
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
    if (!ctx || !listener || ctx->state != SERVICE_STATE_INIT) {
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
    if (ctx && ctx->state == SERVICE_STATE_INIT) {
        ctx->handler = handler;
    }
}

// 启动服务
infra_error_t poly_poll_start(poly_poll_context_t* ctx) {
    if (!ctx || !ctx->handler || ctx->state != SERVICE_STATE_INIT) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    ctx->state = SERVICE_STATE_RUNNING;
    
    // 创建工作线程
    for (int i = 0; i < 4; i++) {  // 创建4个工作线程
        thread_context_t* thread_ctx = malloc(sizeof(*thread_ctx));
        if (!thread_ctx) {
            ctx->state = SERVICE_STATE_ERROR;
            return INFRA_ERROR_NO_MEMORY;
        }
        
        thread_ctx->thread_id = i;
        thread_ctx->ctx = ctx;
        thread_ctx->running = false;
        thread_ctx->next = NULL;
        
        // 添加到线程管理器
        infra_error_t err = add_thread(&ctx->thread_mgr, thread_ctx);
        if (err != INFRA_OK) {
            free(thread_ctx);
            ctx->state = SERVICE_STATE_ERROR;
            return err;
        }
        
        err = infra_thread_pool_submit(ctx->thread_pool, thread_worker, thread_ctx);
        if (err != INFRA_OK) {
            remove_thread(&ctx->thread_mgr, thread_ctx);
            free(thread_ctx);
            ctx->state = SERVICE_STATE_ERROR;
            return err;
        }
    }
    
    // 创建accept协程
    infra_async_create(accept_coroutine, ctx);
    
    // 主线程运行协程调度器
    while (ctx->state == SERVICE_STATE_RUNNING) {
        infra_async_run();
    }
    
    return ctx->state == SERVICE_STATE_ERROR ? 
           INFRA_ERROR_INTERNAL : INFRA_OK;
}

// 停止服务
infra_error_t poly_poll_stop(poly_poll_context_t* ctx) {
    if (!ctx || ctx->state != SERVICE_STATE_RUNNING) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    INFRA_LOG_INFO("Stopping service...");
    
    // 1. 标记停止
    ctx->state = SERVICE_STATE_STOPPING;
    
    // 2. 关闭所有监听socket
    for (size_t i = 0; i < ctx->poll_count; i++) {
        infra_net_close((infra_socket_t)(intptr_t)ctx->poll_fds[i].fd);
        ctx->poll_fds[i].fd = -1;
    }
    
    // 3. 通知所有工作线程停止
    infra_mutex_lock(&ctx->thread_mgr.mutex);
    for (thread_context_t* thread = ctx->thread_mgr.head; thread; thread = thread->next) {
        thread->running = false;
    }
    infra_mutex_unlock(&ctx->thread_mgr.mutex);
    
    // 4. 等待所有线程退出
    int wait_count = 0;
    while (ctx->thread_mgr.count > 0 && wait_count < 100) {  // 最多等待10秒
        infra_sleep_ms(100);
        wait_count++;
    }
    
    // 5. 等待活跃协程完成
    wait_count = 0;
    while (ctx->active_coroutines > 0 && wait_count < 100) {  // 最多等待10秒
        infra_sleep_ms(100);
        wait_count++;
    }
    
    ctx->state = SERVICE_STATE_STOPPED;
    INFRA_LOG_INFO("Service stopped, remaining coroutines: %d, threads: %zu", 
                   ctx->active_coroutines, ctx->thread_mgr.count);
    
    return INFRA_OK;
}

// 清理资源
void poly_poll_cleanup(poly_poll_context_t* ctx) {
    if (!ctx) {
        return;
    }
    
    INFRA_LOG_INFO("Cleaning up resources...");
    
    // 1. 确保服务已停止
    if (ctx->state == SERVICE_STATE_RUNNING) {
        poly_poll_stop(ctx);
    }
    
    // 2. 销毁线程池
    if (ctx->thread_pool) {
        infra_thread_pool_destroy(ctx->thread_pool);
        ctx->thread_pool = NULL;
    }
    
    // 3. 清理线程管理器
    infra_mutex_destroy(&ctx->thread_mgr.mutex);
    
    // 4. 释放poll数组
    if (ctx->poll_fds) {
        // 确保所有fd都已关闭
        for (size_t i = 0; i < ctx->poll_count; i++) {
            if (ctx->poll_fds[i].fd != -1) {
                infra_net_close((infra_socket_t)(intptr_t)ctx->poll_fds[i].fd);
            }
        }
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
    ctx->state = SERVICE_STATE_INIT;
    
    INFRA_LOG_INFO("Cleanup completed");
}
