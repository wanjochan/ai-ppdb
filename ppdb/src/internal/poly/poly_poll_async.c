#include "internal/poly/poly_poll_async.h"

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
}

// 处理单个监听器的协程
static void handle_listener(void* arg) {
    struct {
        infra_socket_t listener;
        void* user_data;
        poly_poll_handler_fn handler;
        poly_poll_context_t* ctx;
    }* args = arg;
    
    while (args->ctx->running) {
        // 接受新连接
        infra_socket_t client;
        infra_error_t err = infra_net_accept(args->listener, &client);
        if (err != INFRA_OK) {
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                infra_yield();  // 无连接时让出CPU
                continue;
            }
            break;  // 其他错误退出
        }
        
        // 为新连接创建协程
        struct {
            infra_socket_t client;
            void* user_data;
            poly_poll_handler_fn handler;
        }* client_args = infra_alloc(sizeof(*client_args));
        
        client_args->client = client;
        client_args->user_data = args->user_data;
        client_args->handler = args->handler;
        
        // 分配到某个线程的协程调度器
        schedule_coroutine(args->ctx, handle_client, client_args);
    }
}

// 线程工作函数
static void thread_worker(void* arg) {
    thread_scheduler_t* sched = arg;
    poly_poll_context_t* ctx = sched->user_data;
    
    // 创建线程特定调度器
    infra_scheduler_t* async_sched = infra_scheduler_create(sched->thread_id);
    if (!async_sched) return;
    
    // 设置为当前线程的调度器
    infra_scheduler_set_current(async_sched);
    
    while (ctx->running) {
        // 运行当前线程的协程调度器
        infra_run_in(async_sched);
        
        // 尝试从其他线程窃取任务
        bool stole = false;
        for (int i = 0; i < ctx->thread_count && !stole; i++) {
            if (i == sched->thread_id) continue;
            
            thread_scheduler_t* victim = &ctx->schedulers[i];
            infra_scheduler_t* victim_sched = victim->user_data;
            if (victim_sched) {
                stole = infra_scheduler_steal(victim_sched, async_sched);
            }
        }
        
        // 无任务时短暂休眠
        if (!async_sched->ready && !stole) {
            infra_sleep_ms(1);
        }
    }
    
    // 清理
    infra_scheduler_destroy(async_sched);
}

// 调度协程到线程
static void schedule_coroutine(poly_poll_context_t* ctx, 
                             infra_async_fn fn, void* arg) {
    static int next_thread = 0;
    
    // 轮询选择线程
    thread_scheduler_t* sched = &ctx->schedulers[next_thread];
    next_thread = (next_thread + 1) % ctx->thread_count;
    
    // 获取线程的调度器
    infra_scheduler_t* async_sched = sched->user_data;
    if (!async_sched) return;
    
    // 创建协程
    infra_go_in(async_sched, fn, arg);
}

// 初始化
infra_error_t poly_poll_init(poly_poll_context_t* ctx, 
                           const poly_poll_config_t* config) {
    if (!ctx || !config) return INFRA_ERROR_INVALID_PARAM;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->max_listeners = config->max_listeners;
    ctx->read_buffer_size = config->read_buffer_size;
    
    // 分配监听器数组
    ctx->listeners = malloc(config->max_listeners * sizeof(infra_socket_t));
    ctx->configs = malloc(config->max_listeners * sizeof(poly_poll_listener_t));
    
    if (!ctx->listeners || !ctx->configs) {
        poly_poll_cleanup(ctx);
        return INFRA_ERROR_NO_MEMORY;
    }
    
    // 创建线程池
    infra_thread_pool_config_t pool_config = {
        .min_threads = config->min_threads,
        .max_threads = config->max_threads,
        .queue_size = config->queue_size
    };
    
    infra_error_t err = infra_thread_pool_create(&pool_config, &ctx->pool);
    if (err != INFRA_OK) {
        poly_poll_cleanup(ctx);
        return err;
    }
    
    // 创建线程调度器
    ctx->thread_count = config->min_threads;
    ctx->schedulers = calloc(ctx->thread_count, sizeof(thread_scheduler_t));
    if (!ctx->schedulers) {
        poly_poll_cleanup(ctx);
        return INFRA_ERROR_NO_MEMORY;
    }
    
    // 初始化并启动工作线程
    for (int i = 0; i < ctx->thread_count; i++) {
        ctx->schedulers[i].thread_id = i;
        ctx->schedulers[i].user_data = infra_scheduler_create(i);
        if (!ctx->schedulers[i].user_data) {
            poly_poll_cleanup(ctx);
            return INFRA_ERROR_NO_MEMORY;
        }
        
        err = infra_thread_pool_submit(ctx->pool, thread_worker, 
                                     &ctx->schedulers[i]);
        if (err != INFRA_OK) {
            poly_poll_cleanup(ctx);
            return err;
        }
    }
    
    return INFRA_OK;
}

// 添加监听器
infra_error_t poly_poll_add_listener(poly_poll_context_t* ctx, 
                                   const poly_poll_listener_t* listener) {
    if (!ctx || !listener) return INFRA_ERROR_INVALID_PARAM;
    if (ctx->listener_count >= ctx->max_listeners) return INFRA_ERROR_NO_SPACE;
    
    // 创建监听socket
    infra_socket_t sock;
    infra_error_t err = infra_net_create(&sock, true, NULL);  // 非阻塞模式
    if (err != INFRA_OK) return err;
    
    // 设置地址重用
    err = infra_net_set_reuseaddr(sock, true);
    if (err != INFRA_OK) {
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
        infra_net_close(sock);
        return err;
    }
    
    // 开始监听
    err = infra_net_listen(sock);
    if (err != INFRA_OK) {
        infra_net_close(sock);
        return err;
    }
    
    // 保存配置
    ctx->listeners[ctx->listener_count] = sock;
    memcpy(&ctx->configs[ctx->listener_count], listener, sizeof(*listener));
    ctx->listener_count++;
    
    return INFRA_OK;
}

// 设置处理函数
void poly_poll_set_handler(poly_poll_context_t* ctx, poly_poll_handler_fn handler) {
    ctx->handler = handler;
}

// 启动服务
infra_error_t poly_poll_start(poly_poll_context_t* ctx) {
    if (!ctx || !ctx->handler) return INFRA_ERROR_INVALID_PARAM;
    if (ctx->running) return INFRA_ERROR_ALREADY_EXISTS;
    
    ctx->running = true;
    
    // 为每个监听器创建协程
    for (int i = 0; i < ctx->listener_count; i++) {
        struct {
            infra_socket_t listener;
            void* user_data;
            poly_poll_handler_fn handler;
            poly_poll_context_t* ctx;
        }* args = infra_alloc(sizeof(*args));
        
        args->listener = ctx->listeners[i];
        args->user_data = ctx->configs[i].user_data;
        args->handler = ctx->handler;
        args->ctx = ctx;
        
        schedule_coroutine(ctx, handle_listener, args);
    }
    
    return INFRA_OK;
}

// 停止服务
infra_error_t poly_poll_stop(poly_poll_context_t* ctx) {
    if (!ctx) return INFRA_ERROR_INVALID_PARAM;
    ctx->running = false;
    return INFRA_OK;
}

// 清理资源
void poly_poll_cleanup(poly_poll_context_t* ctx) {
    if (!ctx) {
        INFRA_LOG_ERROR("Invalid context");
        return;
    }

    INFRA_LOG_INFO("Cleaning up poly_poll_async resources");

    // 停止服务
    ctx->running = false;

    // 等待线程池结束
    if (ctx->pool) {
        infra_thread_pool_destroy(ctx->pool);
        ctx->pool = NULL;
    }

    // 清理调度器
    if (ctx->schedulers) {
        for (int i = 0; i < ctx->thread_count; i++) {
            if (ctx->schedulers[i].user_data) {
                infra_scheduler_destroy(ctx->schedulers[i].user_data);
                ctx->schedulers[i].user_data = NULL;
            }
        }
        free(ctx->schedulers);
        ctx->schedulers = NULL;
    }

    // 关闭所有监听socket
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

    // 清理配置数组
    if (ctx->configs) {
        free(ctx->configs);
        ctx->configs = NULL;
    }

    ctx->listener_count = 0;
    ctx->max_listeners = 0;
    ctx->handler = NULL;
    ctx->thread_count = 0;

    INFRA_LOG_INFO("Poly_poll_async cleanup completed");
}
