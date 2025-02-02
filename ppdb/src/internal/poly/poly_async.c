#include "poly_async.h"
#include <libdill.h>
#include <stdlib.h>
#include <errno.h>

// 内部使用的操作句柄
typedef struct {
    poly_async_op_t op;      // 操作类型
    int coroutine;           // 协程句柄
    int status;              // 操作状态
    size_t bytes;           // 传输的字节数
} async_handle_t;

// 异步上下文的完整定义
struct poly_async_context {
    int bundle;             // 协程束
    bool running;           // 运行状态
};

// 工作协程函数
coroutine void worker(async_handle_t* handle, int timeout_ms) {
    switch(handle->op) {
        case POLY_ASYNC_OP_WAIT:
            // 简单让出CPU
            handle->status = yield();
            break;
            
        case POLY_ASYNC_OP_TIMEOUT:
            handle->status = msleep(timeout_ms);
            break;
            
        case POLY_ASYNC_OP_SIGNAL:
            handle->status = sigtrace();
            break;
            
        default:
            handle->status = -EINVAL;
            break;
    }
}

poly_async_context_t* poly_async_create(void) {
    poly_async_context_t* ctx = calloc(1, sizeof(*ctx));
    if(!ctx) return NULL;
    
    ctx->bundle = bundle();
    if(ctx->bundle < 0) {
        free(ctx);
        return NULL;
    }
    
    ctx->running = true;
    return ctx;
}

void poly_async_destroy(poly_async_context_t* ctx) {
    if(!ctx) return;
    
    ctx->running = false;
    
    if(ctx->bundle >= 0) {
        hclose(ctx->bundle);
    }
    
    free(ctx);
}

poly_async_result_t poly_async_wait(poly_async_context_t* ctx, 
                                  poly_async_op_t op,
                                  int timeout_ms) {
    poly_async_result_t result = {0};
    
    if(!ctx) {
        result.status = -EINVAL;
        return result;
    }
    
    // 创建操作句柄
    async_handle_t* handle = calloc(1, sizeof(*handle));
    if(!handle) {
        result.status = -ENOMEM;
        return result;
    }
    
    handle->op = op;
    
    // 启动工作协程
    handle->coroutine = go(worker(handle, timeout_ms));
    if(handle->coroutine < 0) {
        free(handle);
        result.status = -errno;
        return result;
    }
    
    // 加入bundle
    bundle_go(ctx->bundle, handle->coroutine);
    
    // 等待操作完成
    int rc = bundle_wait(ctx->bundle, timeout_ms);
    if(rc < 0) {
        result.status = -errno;
    } else {
        result.status = handle->status;
        result.bytes = handle->bytes;
    }
    
    free(handle);
    return result;
}

int poly_async_run(poly_async_context_t* ctx) {
    if(!ctx) return -1;
    
    while(ctx->running) {
        int rc = bundle_wait(ctx->bundle, -1);
        if(rc < 0 && errno != EINTR) {
            return -1;
        }
    }
    
    return 0;
}