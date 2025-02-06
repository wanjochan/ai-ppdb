#include "InfraxCore.h"
#include "InfraxAsync.h"
#include "InfraxLog.h"
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <time.h>

// Error codes
#define INFRAX_ERROR_INVALID_COROUTINE INFRAX_ERROR_INVALID_PARAM

// 全局队列
static struct {
    InfraxAsync* ready;      // 就绪队列
    InfraxAsync* current;    // 当前运行的协程
    jmp_buf env;            // 调度器环境
    bool is_running;        // 调度器是否正在运行
    InfraxAsync* cleanup;   // 待清理的协程队列
} g_queue = {0};

// 增加引用计数
static void ref_coroutine(InfraxAsync* co) {
    if (co) {
        co->ref_count++;
    }
}

// 减少引用计数，如果计数为0则释放
static void unref_coroutine(InfraxAsync* co) {
    if (co && --co->ref_count == 0) {
        // 先清理所有子协程
        while (co->first_child) {
            InfraxAsync* child = co->first_child;
            co->first_child = child->next_sibling;
            unref_coroutine(child);
        }
        
        // 从父协程的子列表中移除
        if (co->parent) {
            if (co->parent->first_child == co) {
                co->parent->first_child = co->next_sibling;
            } else {
                InfraxAsync* sibling = co->parent->first_child;
                while (sibling && sibling->next_sibling != co) {
                    sibling = sibling->next_sibling;
                }
                if (sibling) {
                    sibling->next_sibling = co->next_sibling;
                }
            }
        }
        
        // 释放协程本身
        InfraxAsync_CLASS.free(co);
    }
}

// 对齐栈指针
static void* align_stack_ptr(void* ptr, size_t align) {
    uintptr_t addr = (uintptr_t)ptr;
    return (void*)((addr + align - 1) & ~(align - 1));
}

// 分配协程栈空间
static void* alloc_coroutine_stack(size_t size, size_t* actual_size, void** raw_mem) {
    // 确保最小栈大小
    if (size < INFRAX_MIN_STACK_SIZE) {
        size = INFRAX_MIN_STACK_SIZE;
    }
    
    // 分配额外空间用于对齐和保护区
    size_t total_size = size + INFRAX_STACK_ALIGN + 2 * INFRAX_STACK_REDZONE;
    void* mem = malloc(total_size);
    if (!mem) {
        return NULL;
    }
    
    // 对齐栈基地址
    void* stack_base = align_stack_ptr((char*)mem + INFRAX_STACK_REDZONE, INFRAX_STACK_ALIGN);
    *actual_size = size;
    *raw_mem = mem;  // 保存原始内存指针用于释放
    
    // 在保护区填充特殊模式以检测栈溢出
    memset(mem, 0xCC, INFRAX_STACK_REDZONE);
    memset((char*)stack_base + size, 0xCC, INFRAX_STACK_REDZONE);
    
    return stack_base;
}

// Create new coroutine
static InfraxAsync* async_new(const InfraxAsyncConfig* config) {
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Creating new coroutine: %s", config ? config->name : "NULL");
    
    if (!config || !config->fn) {
        log->error(log, "Invalid coroutine config: fn=%p",
                  config ? config->fn : NULL);
        return NULL;
    }

    // 分配协程控制块
    InfraxAsync* self = malloc(sizeof(InfraxAsync));
    if (!self) {
        log->error(log, "Failed to allocate coroutine control block");
        return NULL;
    }
    log->debug(log, "Allocated coroutine control block at %p", self);
    
    memset(self, 0, sizeof(InfraxAsync));
    self->klass = &InfraxAsync_CLASS;
    self->config = *config;
    self->state = COROUTINE_INIT;
    self->type = ASYNC_NONE;  // 初始化为NONE类型
    
    // 设置实例方法
    self->start = async_start;
    self->yield = async_yield;
    self->resume = async_resume;
    self->is_done = async_is_done;
    
    // 分配并初始化协程栈
    size_t actual_size;
    void* raw_mem;
    self->stack = alloc_coroutine_stack(config->stack_size ? config->stack_size : DEFAULT_STACK_SIZE, 
                                      &actual_size, &raw_mem);
    if (!self->stack) {
        log->error(log, "Failed to allocate coroutine stack");
        free(self);
        return NULL;
    }
    self->stack_size = actual_size;
    self->raw_stack = raw_mem;  // 保存原始栈指针
    log->debug(log, "Allocated coroutine stack at %p, size=%zu", self->stack, self->stack_size);
    
    // 初始化栈顶指针
    self->stack_top = (char*)self->stack + self->stack_size;
    
    // 初始化引用计数
    self->ref_count = 1;
    
    log->debug(log, "Created coroutine %s successfully", config->name);
    return self;
}

// Free coroutine
static void async_free(InfraxAsync* self) {
    if (!self) return;
    
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Freeing coroutine %s", self->config.name);
    
    // 释放协程栈
    if (self->raw_stack) {
        free(self->raw_stack);
        self->raw_stack = NULL;
        self->stack = NULL;
        self->stack_size = 0;
        self->stack_top = NULL;
    }
    
    // 释放控制块
    free(self);
}

// Start coroutine
static InfraxError async_start(InfraxAsync* self) {
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Starting coroutine %s (state=%d)", self->config.name, self->state);
    
    if (!self || self->state != COROUTINE_INIT) {
        log->error(log, "Invalid coroutine state for start: %d", self ? self->state : -1);
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid coroutine state");
    }
    
    // 加入就绪队列
    self->next = g_queue.ready;
    g_queue.ready = self;
    self->state = COROUTINE_READY;
    log->debug(log, "Coroutine %s added to ready queue", self->config.name);
    
    return INFRAX_ERROR_OK_STRUCT;
}

// Yield execution
static InfraxError async_yield(InfraxAsync* self) {
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Yielding coroutine %s (state=%d)", self->config.name, self->state);
    
    if (!self || self->state != COROUTINE_RUNNING) {
        log->error(log, "Invalid coroutine state for yield: %d", self ? self->state : -1);
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid coroutine state");
    }
    
    // 检查是否需要等待事件
    if (self->type != ASYNC_NONE) {
        int ret = internal_poll(self);
        if (ret == 0) {
            // 事件已就绪，无需让出
            log->debug(log, "Event ready for coroutine %s", self->config.name);
            return INFRAX_ERROR_OK_STRUCT;
        }
        // 需要继续等待
        self->state = COROUTINE_YIELDED;
        // 保存协程上下文并切换到调度器
        if (setjmp(self->env) == 0) {
            longjmp(g_queue.env, 1);
        }
        // 恢复后继续执行
        self->state = COROUTINE_RUNNING;
        return INFRAX_ERROR_OK_STRUCT;
    }
    
    // 保存协程上下文
    if (setjmp(self->env) == 0) {
        log->debug(log, "Saved context for coroutine %s, switching to scheduler", self->config.name);
        self->state = COROUTINE_YIELDED;
        longjmp(g_queue.env, 1);
    }
    
    // 恢复后继续执行
    self->state = COROUTINE_RUNNING;
    log->debug(log, "Resumed coroutine %s after yield", self->config.name);
    return INFRAX_ERROR_OK_STRUCT;
}

// Resume coroutine
static InfraxError async_resume(InfraxAsync* self) {
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Resuming coroutine %s (state=%d)", self->config.name, self->state);
    
    if (!self || self->state != COROUTINE_YIELDED) {
        log->error(log, "Invalid coroutine state for resume: %d", self ? self->state : -1);
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid coroutine state");
    }
    
    // 检查是否需要等待事件
    if (self->type != ASYNC_NONE) {
        int ret = internal_poll(self);
        if (ret != 0) {
            // 需要继续等待
            return INFRAX_ERROR_OK_STRUCT;
        }
    }
    
    // 设置为就绪状态，等待调度器运行
    self->state = COROUTINE_READY;
    
    // 加入就绪队列
    self->next = g_queue.ready;
    g_queue.ready = self;
    log->debug(log, "Coroutine %s added to ready queue", self->config.name);
    
    return INFRAX_ERROR_OK_STRUCT;
}

// Check if coroutine is done
static bool async_is_done(const InfraxAsync* self) {
    if (!self) return true;
    return self->state == COROUTINE_DONE;
}

// 将协程加入待清理队列
static void queue_for_cleanup(InfraxAsync* co) {
    if (!co) return;
    
    // 从就绪队列中移除
    if (g_queue.ready == co) {
        g_queue.ready = co->next;
    } else {
        InfraxAsync* prev = g_queue.ready;
        while (prev && prev->next != co) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = co->next;
        }
    }
    
    // 加入清理队列
    co->next = g_queue.cleanup;
    g_queue.cleanup = co;
}

// 清理完成的协程
static void cleanup_done_coroutines(void) {
    while (g_queue.cleanup) {
        InfraxAsync* co = g_queue.cleanup;
        g_queue.cleanup = co->next;
        unref_coroutine(co);
    }
}

// Run coroutines
void InfraxAsyncRun(void) {
    InfraxLog* log = get_global_infrax_log();
    
    // 防止重入
    if (g_queue.is_running) {
        log->warn(log, "InfraxAsyncRun: Already running, skipping");
        return;
    }
    
    log->debug(log, "InfraxAsyncRun: Starting");
    g_queue.is_running = true;
    
    // 处理所有就绪协程
    while (g_queue.ready) {
        // 取出一个就绪协程
        InfraxAsync* co = g_queue.ready;
        g_queue.ready = co->next;
        co->next = NULL;
        
        log->debug(log, "Running coroutine: %s (state: %d)", co->config.name, co->state);
        
        // 保存调度器上下文并运行协程
        InfraxAsync* prev_current = g_queue.current;
        g_queue.current = co;
        
        if (co->state == COROUTINE_READY) {
            // 首次运行协程
            log->debug(log, "Starting coroutine function: %s", co->config.name);
            co->state = COROUTINE_RUNNING;
            co->config.fn(co->config.arg);
            // 如果协程没有yield，则标记为完成
            if (co->state == COROUTINE_RUNNING) {
                co->state = COROUTINE_DONE;
                log->debug(log, "Coroutine completed: %s", co->config.name);
                queue_for_cleanup(co);
            }
        } else if (co->state == COROUTINE_YIELDED) {
            // 恢复之前让出的协程
            if (setjmp(g_queue.env) == 0) {
                log->debug(log, "Resuming coroutine: %s", co->config.name);
                co->state = COROUTINE_RUNNING;
                longjmp(co->env, 1);
            }
            
            // 协程再次让出或完成
            if (co->state == COROUTINE_RUNNING) {
                // 协程没有显式让出，视为完成
                co->state = COROUTINE_DONE;
                log->debug(log, "Coroutine completed: %s", co->config.name);
                queue_for_cleanup(co);
            } else if (co->state == COROUTINE_YIELDED) {
                log->debug(log, "Coroutine yielded: %s", co->config.name);
                // 将让出的协程重新加入就绪队列末尾
                InfraxAsync** tail = &g_queue.ready;
                while (*tail) {
                    tail = &(*tail)->next;
                }
                *tail = co;
            }
        }
        
        // 恢复之前的当前协程
        g_queue.current = prev_current;
    }
    
    // 清理完成的协程
    cleanup_done_coroutines();
    
    g_queue.is_running = false;
    log->debug(log, "InfraxAsyncRun: Finished");
}

// 获取当前时间戳(毫秒)
static uint64_t get_monotonic_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

// 内部轮询实现
static int internal_poll(InfraxAsync* self) {
    if (!self) return -1;
    
    switch (self->type) {
        case ASYNC_IO: {
            struct pollfd pfd = {0};
            pfd.fd = self->params.io.fd;
            
            // 转换事件标志
            if (self->params.io.events & INFRAX_IO_READ)
                pfd.events |= POLLIN;
            if (self->params.io.events & INFRAX_IO_WRITE)
                pfd.events |= POLLOUT;
            if (self->params.io.events & INFRAX_IO_ERROR)
                pfd.events |= POLLERR;
            
            // 非阻塞检查
            int ret = poll(&pfd, 1, 0);
            
            if (ret > 0) {
                // 检查返回的事件
                if ((self->params.io.events & INFRAX_IO_READ) && (pfd.revents & POLLIN))
                    return 0;
                if ((self->params.io.events & INFRAX_IO_WRITE) && (pfd.revents & POLLOUT))
                    return 0;
                if ((self->params.io.events & INFRAX_IO_ERROR) && (pfd.revents & POLLERR))
                    return 0;
            }
            return 1;  // 继续等待
        }
        
        case ASYNC_TIMER: {
            // 使用协程自己的字段来存储开始时间
            if (self->params.timer.start_time == 0) {
                self->params.timer.start_time = get_monotonic_ms();
                return 1;  // 继续等待
            }
            
            uint64_t now = get_monotonic_ms();
            if (now - self->params.timer.start_time >= (uint64_t)self->params.timer.ms) {
                return 0;  // 定时器到期
            }
            return 1;  // 继续等待
        }
        
        default:
            return 0;  // 无需等待
    }
}

// 设置父子关系
static void set_parent(InfraxAsync* child, InfraxAsync* parent) {
    if (!child || !parent) return;
    
    // 如果已经有父协程，先解除关系
    if (child->parent) {
        if (child->parent->first_child == child) {
            child->parent->first_child = child->next_sibling;
        } else {
            InfraxAsync* sibling = child->parent->first_child;
            while (sibling && sibling->next_sibling != child) {
                sibling = sibling->next_sibling;
            }
            if (sibling) {
                sibling->next_sibling = child->next_sibling;
            }
        }
        unref_coroutine(child);  // 减少旧父协程的引用
    }
    
    // 设置新的父子关系
    child->parent = parent;
    child->next_sibling = parent->first_child;
    parent->first_child = child;
    ref_coroutine(child);  // 增加新父协程的引用
}

// 定时器协程函数
static void timer_coroutine(void* arg) {
    InfraxAsync* self = (InfraxAsync*)arg;
    InfraxLog* log = get_global_infrax_log();
    
    log->debug(log, "Timer started: %d ms", self->params.timer.ms);
    
    // 初始化定时器
    self->type = ASYNC_TIMER;
    self->params.timer.start_time = 0;  // 将在第一次 poll 时设置
    
    // 等待直到时间到
    while (1) {
        InfraxError err = self->yield(self);
        if (!INFRAX_ERROR_IS_OK(err)) {
            log->error(log, "Timer yield failed");
            break;
        }
        
        // 如果 yield 返回成功且没有抛出，说明定时器已完成
        log->debug(log, "Timer completed");
        self->state = COROUTINE_DONE;  // 主动设置完成状态
        break;
    }
}

// Create a timer coroutine
InfraxAsync* InfraxAsync_CreateTimer(int ms) {
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "Creating timer coroutine for %d ms", ms);
    
    // 创建定时器协程
    InfraxAsyncConfig config = {
        .name = "timer",
        .fn = timer_coroutine,
        .arg = NULL,
        .stack_size = DEFAULT_STACK_SIZE
    };
    
    InfraxAsync* timer = InfraxAsync_CLASS.new(&config);
    if (!timer) {
        log->error(log, "Failed to create timer coroutine");
        return NULL;
    }
    
    // 设置定时器参数
    timer->type = ASYNC_TIMER;
    timer->params.timer.ms = ms;
    timer->params.timer.start_time = 0;
    
    // 设置父子关系
    if (g_queue.current) {
        set_parent(timer, g_queue.current);
    }
    
    return timer;
}

InfraxAsync* InfraxAsync_CreateIO(int fd, int events) {
    InfraxAsyncConfig config = {
        .name = "io",
        .fn = NULL,  // 需要设置实际的IO回调函数
        .arg = NULL
    };
    
    InfraxAsync* async = InfraxAsync_CLASS.new(&config);
    if (async) {
        async->type = ASYNC_IO;
        async->params.io.fd = fd;
        async->params.io.events = events;
    }
    return async;
}

// Class instance
const InfraxAsyncClass InfraxAsync_CLASS = {
    .new = async_new,
    .free = async_free
};
