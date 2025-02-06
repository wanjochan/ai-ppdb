#include "InfraxCore.h"
#include "InfraxAsync.h"
#include "InfraxLog.h"
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>

// Error codes
#define INFRAX_ERROR_INVALID_COROUTINE INFRAX_ERROR_INVALID_PARAM

// Thread-local coroutine queue
static __thread struct {
    InfraxAsync* ready;    // Ready queue head
    InfraxAsync* current;  // Currently running coroutine
    jmp_buf env;          // Scheduler context
} g_queue = {0};

// Forward declarations
static InfraxError async_start(InfraxAsync* self);
static InfraxError async_yield(InfraxAsync* self);
static InfraxError async_resume(InfraxAsync* self);
static bool async_is_done(const InfraxAsync* self);
static int internal_poll(InfraxAsync* self);

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
        if (ret != 0) {
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
    
    // 加入就绪队列
    self->next = g_queue.ready;
    g_queue.ready = self;
    self->state = COROUTINE_RUNNING;  // 直接设置为运行状态
    log->debug(log, "Coroutine %s added to ready queue", self->config.name);
    
    return INFRAX_ERROR_OK_STRUCT;
}

// Check if coroutine is done
static bool async_is_done(const InfraxAsync* self) {
    return self && self->state == COROUTINE_DONE;
}

// Run coroutines
void InfraxAsyncRun(void) {
    InfraxLog* log = get_global_infrax_log();
    log->debug(log, "InfraxAsyncRun: Starting");
    
    // 如果没有就绪协程，返回
    if (!g_queue.ready) {
        log->debug(log, "No ready coroutines, returning");
        return;
    }
    
    // 取出一个就绪协程
    InfraxAsync* co = g_queue.ready;
    g_queue.ready = co->next;
    co->next = NULL;
    
    log->debug(log, "Running coroutine: %s (state: %d)", co->config.name, co->state);
    
    // 保存调度器上下文并运行协程
    InfraxAsync* prev_current = g_queue.current;
    g_queue.current = co;
    
    if (setjmp(g_queue.env) == 0) {
        if (co->state == COROUTINE_READY) {
            log->debug(log, "Starting coroutine function: %s", co->config.name);
            co->state = COROUTINE_RUNNING;
            co->config.fn(co->config.arg);
            co->state = COROUTINE_DONE;
            log->debug(log, "Coroutine completed: %s", co->config.name);
            // 协程完成时恢复调度器上下文
            g_queue.current = prev_current;
            return;  // 直接返回，不需要longjmp
        } else if (co->state == COROUTINE_RUNNING) {
            log->debug(log, "Resuming coroutine: %s", co->config.name);
            longjmp(co->env, 1);
        }
    }
    
    // 协程已让出或完成
    if (co->state == COROUTINE_DONE) {
        log->debug(log, "Coroutine done: %s", co->config.name);
        g_queue.current = prev_current;
        // 释放已完成的协程
        InfraxAsync_CLASS.free(co);
    } else if (co->state == COROUTINE_YIELDED) {
        log->debug(log, "Coroutine yielded: %s", co->config.name);
        // 将让出的协程重新加入就绪队列
        co->next = g_queue.ready;
        g_queue.ready = co;
    }
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
            fd_set readfds, writefds, errorfds;
            FD_ZERO(&readfds);
            FD_ZERO(&writefds);
            FD_ZERO(&errorfds);
            
            // 根据事件类型设置fd
            if (self->params.io.events & INFRAX_IO_READ)
                FD_SET(self->params.io.fd, &readfds);
            if (self->params.io.events & INFRAX_IO_WRITE)
                FD_SET(self->params.io.fd, &writefds);
            if (self->params.io.events & INFRAX_IO_ERROR)
                FD_SET(self->params.io.fd, &errorfds);
            
            struct timeval tv = {0, 0};  // 非阻塞检查
            int ret = select(self->params.io.fd + 1, &readfds, &writefds, &errorfds, &tv);
            
            if (ret > 0) {
                // 检查是否有我们关心的事件
                if ((self->params.io.events & INFRAX_IO_READ) && FD_ISSET(self->params.io.fd, &readfds))
                    return 0;
                if ((self->params.io.events & INFRAX_IO_WRITE) && FD_ISSET(self->params.io.fd, &writefds))
                    return 0;
                if ((self->params.io.events & INFRAX_IO_ERROR) && FD_ISSET(self->params.io.fd, &errorfds))
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
                self->params.timer.start_time = 0;  // 重置定时器
                return 0;  // 定时器到期
            }
            return 1;  // 继续等待
        }
        
        default:
            return 0;  // 无需等待
    }
}

// 定时器协程函数
static void timer_coroutine(void* arg) {
    InfraxAsync* self = (InfraxAsync*)arg;
    // 等待指定时间
    self->yield(self);
}

// 工厂函数实现
InfraxAsync* InfraxAsync_CreateTimer(int ms) {
    InfraxAsyncConfig config = {
        .name = "timer",
        .fn = timer_coroutine,  // 使用定时器协程函数
        .arg = NULL,  // 在async_new中会设置为self
        .stack_size = DEFAULT_STACK_SIZE
    };
    
    InfraxAsync* async = InfraxAsync_CLASS.new(&config);
    if (async) {
        async->type = ASYNC_TIMER;
        async->params.timer.ms = ms;
        async->config.arg = async;  // 设置协程参数为自身
    }
    return async;
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
