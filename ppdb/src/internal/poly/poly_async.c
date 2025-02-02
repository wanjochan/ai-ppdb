#include "poly_async.h"
#include <stdlib.h>
#include <string.h>

// 全局调度器状态
static struct {
    poly_async_ctx* ready;    // 就绪队列
    poly_async_ctx* current;  // 当前运行的协程
    jmp_buf scheduler;        // 调度器上下文
    
    // 栈使用统计
    struct {
        size_t total_allocs;  // 总分配次数
        size_t grow_count;    // 栈增长次数
        size_t peak_size;     // 峰值大小
    } stats;
} scheduler;

// 获取当前协程
poly_async_ctx* poly_current(void) {
    return scheduler.current;
}

// 智能栈增长
static int grow_stack(poly_async_ctx* ctx) {
    scheduler.stats.grow_count++;
    
    // 计算新大小：根据增长频率和当前使用量智能调整
    size_t grow_factor = scheduler.stats.grow_count > 3 ? 4 : 2;
    size_t new_size = ctx->size * grow_factor;
    
    // 限制最大值
    if (new_size > POLY_STACK_MAX) {
        new_size = POLY_STACK_MAX;
    }
    
    char* new_stack = realloc(ctx->stack, new_size);
    if (!new_stack) return -1;
    
    ctx->stack = new_stack;
    ctx->size = new_size;
    
    // 更新统计
    if (new_size > scheduler.stats.peak_size) {
        scheduler.stats.peak_size = new_size;
    }
    
    return 0;
}

// 创建新协程
poly_async_ctx* poly_go(poly_async_fn fn, void* arg) {
    poly_async_ctx* ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    
    // 智能初始栈大小：根据历史数据
    size_t initial_size = scheduler.stats.peak_size > 0 
        ? (scheduler.stats.peak_size / 2)  // 使用历史峰值的一半
        : POLY_STACK_MIN;                  // 首次使用最小值
    
    if (initial_size < POLY_STACK_MIN) initial_size = POLY_STACK_MIN;
    if (initial_size > POLY_STACK_MAX) initial_size = POLY_STACK_MAX;
    
    ctx->stack = malloc(initial_size);
    if (!ctx->stack) {
        free(ctx);
        return NULL;
    }
    
    ctx->size = initial_size;
    ctx->used = 0;
    ctx->fn = fn;
    ctx->arg = arg;
    ctx->done = 0;
    
    // 加入就绪队列
    ctx->next = scheduler.ready;
    scheduler.ready = ctx;
    
    return ctx;
}

// 在当前协程栈上分配内存
void* poly_alloc(size_t size) {
    poly_async_ctx* ctx = poly_current();
    if (!ctx) return NULL;
    
    scheduler.stats.total_allocs++;
    
    // 对齐到8字节
    size = (size + 7) & ~7;
    
    // 智能增长：预留一些空间避免频繁扩展
    size_t required = ctx->used + size;
    if (required > ctx->size) {
        size_t margin = size * 2;  // 额外预留空间
        while (ctx->used + size + margin > ctx->size) {
            if (grow_stack(ctx) < 0) {
                return NULL;
            }
        }
    }
    
    void* ptr = ctx->stack + ctx->used;
    ctx->used += size;
    return ptr;
}

// 重置当前协程的内存使用
void poly_reset(void) {
    poly_async_ctx* ctx = poly_current();
    if (ctx) {
        ctx->used = 0;
    }
}

// 让出执行权
void poly_yield(void) {
    poly_async_ctx* ctx = poly_current();
    if (!ctx) return;
    
    // 保存当前上下文并切回调度器
    if (!setjmp(ctx->env)) {
        longjmp(scheduler.scheduler, 1);
    }
}

// 运行调度器
void poly_run(void) {
    // 如果没有就绪协程，直接返回
    if (!scheduler.ready) return;
    
    // 保存调度器上下文
    if (!setjmp(scheduler.scheduler)) {
        // 取出一个就绪协程
        poly_async_ctx* ctx = scheduler.ready;
        scheduler.ready = ctx->next;
        scheduler.current = ctx;
        
        // 首次运行或恢复协程
        if (!setjmp(ctx->env)) {
            ctx->fn(ctx->arg);
            // 协程函数返回时自动标记完成
            ctx->done = 1;
        }
        
        if (ctx->done) {
            // 释放协程资源
            free(ctx->stack);
            free(ctx);
        } else {
            // 未完成的协程重新加入队列
            ctx->next = scheduler.ready;
            scheduler.ready = ctx;
        }
        
        scheduler.current = NULL;
    }
}
