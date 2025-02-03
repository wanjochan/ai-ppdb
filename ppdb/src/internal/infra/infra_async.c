
#include "infra_async.h"

// 全局调度器状态
static struct {
    infra_async_ctx* ready;    // 就绪队列
    infra_async_ctx* current;  // 当前运行的协程
    jmp_buf scheduler;        // 调度器上下文
    
    // 栈使用统计
    struct {
        size_t total_allocs;  // 总分配次数
        size_t grow_count;    // 栈增长次数
        size_t peak_size;     // 峰值大小
    } stats;
} scheduler;

// 线程局部存储：当前线程的调度器
static __thread infra_scheduler_t* current_scheduler = NULL;

// 默认调度器
static infra_scheduler_t default_scheduler = {
    .ready = NULL,
    .current = NULL,
    .id = 0,
};

// 获取当前调度器
static infra_scheduler_t* get_scheduler(void) {
    infra_scheduler_t* sched = current_scheduler;
    if (!sched) {
        sched = &default_scheduler;
    }
    return sched;
}

// 获取当前协程
infra_async_ctx* infra_current(void) {
    return scheduler.current;
}

// 智能栈增长
static int grow_stack(infra_async_ctx* ctx) {
    scheduler.stats.grow_count++;
    
    // 计算新大小：根据增长频率和当前使用量智能调整
    size_t grow_factor = scheduler.stats.grow_count > 3 ? 4 : 2;
    size_t new_size = ctx->size * grow_factor;
    
    // 限制最大值
    if (new_size > INFRA_STACK_MAX) {
        new_size = INFRA_STACK_MAX;
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
infra_async_ctx* infra_go(infra_async_fn fn, void* arg) {
    infra_async_ctx* ctx = calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    
    // 智能初始栈大小：根据历史数据
    size_t initial_size = scheduler.stats.peak_size > 0 
        ? (scheduler.stats.peak_size / 2)  // 使用历史峰值的一半
        : INFRA_STACK_MIN;                 // 首次使用最小值
    
    if (initial_size < INFRA_STACK_MIN) initial_size = INFRA_STACK_MIN;
    if (initial_size > INFRA_STACK_MAX) initial_size = INFRA_STACK_MAX;
    
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
void* infra_alloc(size_t size) {
    infra_async_ctx* ctx = infra_current();
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
void infra_reset(void) {
    infra_async_ctx* ctx = infra_current();
    if (ctx) {
        ctx->used = 0;
    }
}

// 让出执行权
infra_error_t infra_yield(void) {
    infra_scheduler_t* sched = get_scheduler();
    if (!sched->current) return INFRA_ERROR_INVALID_STATE;
    
    // 保存当前上下文
    if (setjmp(sched->current->env) == 0) {
        sched->stats.total_yields++;
        // 切回调度器
        longjmp(sched->env, 1);
    }
    
    return INFRA_OK;
}

// 运行调度器
void infra_run(void) {
    // 如果没有就绪协程，直接返回
    if (!scheduler.ready) return;
    
    // 保存调度器上下文
    if (!setjmp(scheduler.scheduler)) {
        // 取出一个就绪协程
        infra_async_ctx* ctx = scheduler.ready;
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

// 创建新的调度器
infra_scheduler_t* infra_scheduler_create(int id) {
    infra_scheduler_t* sched = calloc(1, sizeof(*sched));
    if (!sched) return NULL;
    
    sched->id = id;
    return sched;
}

// 销毁调度器
void infra_scheduler_destroy(infra_scheduler_t* sched) {
    if (!sched || sched == &default_scheduler) return;
    
    // 清理所有协程
    infra_async_ctx* co = sched->ready;
    while (co) {
        infra_async_ctx* next = co->next;
        free(co->stack);
        free(co);
        co = next;
    }
    
    free(sched);
}

// 设置当前线程的调度器
void infra_scheduler_set_current(infra_scheduler_t* sched) {
    current_scheduler = sched;
}

// 获取当前线程的调度器
infra_scheduler_t* infra_scheduler_current(void) {
    return get_scheduler();
}

// 创建新协程（指定调度器）
infra_async_ctx* infra_go_in(infra_scheduler_t* sched,
                            infra_async_fn fn, void* arg) {
    if (!sched) sched = get_scheduler();
    
    // 分配协程结构
    infra_async_ctx* co = malloc(sizeof(*co));
    if (!co) return NULL;
    
    // 分配初始栈
    co->stack = malloc(INFRA_STACK_MIN);
    if (!co->stack) {
        free(co);
        return NULL;
    }
    
    co->size = INFRA_STACK_MIN;
    co->used = 0;
    co->fn = fn;
    co->arg = arg;
    co->done = 0;
    
    // 加入就绪队列尾部
    co->next = NULL;
    if (!sched->ready) {
        sched->ready = co;
    } else {
        infra_async_ctx* last = sched->ready;
        while (last->next) {
            last = last->next;
        }
        last->next = co;
    }
    
    sched->stats.total_allocs++;
    return co;
}

// 创建新协程（默认调度器）
infra_async_ctx* infra_go(infra_async_fn fn, void* arg) {
    return infra_go_in(get_scheduler(), fn, arg);
}

// 让出执行权
infra_error_t infra_yield(void) {
    infra_scheduler_t* sched = get_scheduler();
    if (!sched->current) return INFRA_ERROR_INVALID_STATE;
    
    // 保存当前上下文
    if (setjmp(sched->current->env) == 0) {
        sched->stats.total_yields++;
        // 切回调度器
        longjmp(sched->env, 1);
    }
    
    return INFRA_OK;
}

// 运行指定调度器
void infra_run_in(infra_scheduler_t* sched) {
    if (!sched) sched = get_scheduler();
    
    // 保存调度器上下文
    if (setjmp(sched->env) == 0) {
        goto schedule;
    }
    
    // 从协程返回或yield
    if (sched->current) {
        if (sched->current->done) {
            // 协程结束，释放资源
            infra_async_ctx* done = sched->current;
            sched->current = NULL;
            
            // 从就绪队列移除
            if (sched->ready == done) {
                sched->ready = done->next;
            } else {
                infra_async_ctx* prev = sched->ready;
                while (prev && prev->next != done) {
                    prev = prev->next;
                }
                if (prev) {
                    prev->next = done->next;
                }
            }
            
            free(done->stack);
            free(done);
        }
    }
    
schedule:
    // 选择下一个要运行的协程
    if (!sched->current && sched->ready) {
        sched->current = sched->ready;
        
        if (setjmp(sched->env) == 0) {
            // 首次运行协程
            char* sp = sched->current->stack + sched->current->size;
            
            // 调用协程函数
            __asm__ __volatile__ (
                "movq %0, %%rsp\n"
                "movq %1, %%rdi\n"
                "callq *%2\n"
                :
                : "r"(sp), "r"(sched->current->arg), "r"(sched->current->fn)
                : "rdi"
            );
            
            // 协程函数返回
            sched->current->done = 1;
            longjmp(sched->env, 1);
        }
    }
}

// 运行默认调度器
void infra_run(void) {
    infra_run_in(get_scheduler());
}

// 在当前协程栈上分配内存
void* infra_alloc(size_t size) {
    infra_scheduler_t* sched = get_scheduler();
    if (!sched->current) return NULL;
    
    infra_async_ctx* co = sched->current;
    
    // 检查是否需要增长栈
    if (co->used + size > co->size) {
        size_t new_size = co->size * 2;
        while (new_size < co->used + size) {
            new_size *= 2;
        }
        if (new_size > INFRA_STACK_MAX) return NULL;
        
        // 重新分配栈
        char* new_stack = realloc(co->stack, new_size);
        if (!new_stack) return NULL;
        
        co->stack = new_stack;
        co->size = new_size;
        sched->stats.grow_count++;
        
        if (new_size > sched->stats.peak_size) {
            sched->stats.peak_size = new_size;
        }
    }
    
    // 分配内存
    void* ptr = co->stack + co->used;
    co->used += size;
    return ptr;
}

// 重置当前协程的内存使用
void infra_reset(void) {
    infra_scheduler_t* sched = get_scheduler();
    if (!sched->current) return;
    sched->current->used = 0;
}

// 窃取任务
bool infra_scheduler_steal(infra_scheduler_t* from, infra_scheduler_t* to) {
    if (!from || !to || from == to) return false;
    
    // 尝试窃取一半的任务
    infra_async_ctx* co = from->ready;
    if (!co) return false;
    
    // 计算任务数量
    int count = 0;
    infra_async_ctx* curr = co;
    while (curr) {
        count++;
        curr = curr->next;
    }
    
    // 窃取一半
    int steal_count = count / 2;
    if (steal_count == 0) return false;
    
    // 找到分割点
    curr = co;
    for (int i = 1; i < steal_count; i++) {
        curr = curr->next;
    }
    
    // 转移任务
    from->ready = curr->next;
    curr->next = to->ready;
    to->ready = co;
    
    from->stats.total_steals++;
    return true;
}
