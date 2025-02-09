#include "internal/polyx/PolyxAsync.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxNet.h"
#include "internal/infrax/InfraxCore.h"
#include <errno.h>

// 全局内存管理器
static InfraxMemory* g_memory = NULL;
extern const InfraxMemoryClassType InfraxMemoryClass;

// 全局 Core 实例
static InfraxCore* g_core = NULL;

// 建议的重构方向：
// PolyxAsync 应该只保留最基础的异步原语
// 把 file_* 相关移到 PolyxIO
// 把 http_* 相关移到 PolyxNet
// 每个模块都可以基于 PolyxAsync 的基础设施来实现自己的异步操作

// TODO: InfraxAsync 性能优化完成后再实现这些函数
// 目前主要问题：
// 1. 底层 InfraxAsync 的性能问题需要先解决
// 2. 异步任务的调度和执行效率需要提升
// 3. 完成底层优化后再考虑具体实现或重构方案

// Internal callback functions for InfraxAsync
// These functions are intended to be the actual implementations of async operations
// but implementation is pending InfraxAsync performance optimization

// File operation callbacks
void async_read_file_fn(InfraxAsync* async, void* arg);
void async_write_file_fn(InfraxAsync* async, void* arg);

// HTTP operation callbacks
void async_http_get_fn(InfraxAsync* async, void* arg);
void async_http_post_fn(InfraxAsync* async, void* arg);

// Timer operation callbacks
void async_delay_fn(InfraxAsync* async, void* arg);
void async_interval_fn(InfraxAsync* async, void* arg);

// Task composition callbacks
void async_parallel_fn(InfraxAsync* async, void* arg);
void async_sequence_fn(InfraxAsync* async, void* arg);

// 实例方法声明
PolyxAsync* polyx_async_start(PolyxAsync* self);
void polyx_async_cancel(PolyxAsync* self);
bool polyx_async_is_done(PolyxAsync* self);
void* polyx_async_get_result(PolyxAsync* self, size_t* size);

// 并行和序列执行方法声明
PolyxAsync* polyx_async_parallel_start(PolyxAsync* self);
void polyx_async_parallel_cancel(PolyxAsync* self);
bool polyx_async_parallel_is_done(PolyxAsync* self);
void* polyx_async_parallel_get_result(PolyxAsync* self, size_t* size);

PolyxAsync* polyx_async_sequence_start(PolyxAsync* self);
void polyx_async_sequence_cancel(PolyxAsync* self);
bool polyx_async_sequence_is_done(PolyxAsync* self);
void* polyx_async_sequence_get_result(PolyxAsync* self, size_t* size);

// 任务结构定义
typedef struct {
    char* path;
} FileReadTask;

typedef struct {
    char* path;
    void* data;
    size_t size;
} FileWriteTask;

typedef struct {
    char* url;
} HttpGetTask;

typedef struct {
    char* url;
    void* data;
    size_t size;
} HttpPostTask;

typedef struct {
    int ms;
} DelayTask;

typedef struct {
    int ms;
    int count;
    int current;
} IntervalTask;

// 并行和序列执行数据结构
typedef struct {
    PolyxAsync** tasks;
    int count;
    int completed;
    int current;
} ParallelSequenceData;

// 清理函数定义
static void file_read_task_cleanup(void* private_data) {
    FileReadTask* task = (FileReadTask*)private_data;
    if (!task) return;
    if (task->path) {
        g_memory->dealloc(g_memory, task->path);
    }
    g_memory->dealloc(g_memory, task);
}

static void file_write_task_cleanup(void* private_data) {
    FileWriteTask* task = (FileWriteTask*)private_data;
    if (!task) return;
    if (task->path) {
        g_memory->dealloc(g_memory, task->path);
    }
    if (task->data) {
        g_memory->dealloc(g_memory, task->data);
    }
    g_memory->dealloc(g_memory, task);
}

static void http_get_task_cleanup(void* private_data) {
    HttpGetTask* task = (HttpGetTask*)private_data;
    if (!task) return;
    if (task->url) {
        g_memory->dealloc(g_memory, task->url);
    }
    g_memory->dealloc(g_memory, task);
}

static void http_post_task_cleanup(void* private_data) {
    HttpPostTask* task = (HttpPostTask*)private_data;
    if (!task) return;
    if (task->url) {
        g_memory->dealloc(g_memory, task->url);
    }
    if (task->data) {
        g_memory->dealloc(g_memory, task->data);
    }
    g_memory->dealloc(g_memory, task);
}

static void delay_task_cleanup(void* private_data) {
    DelayTask* task = (DelayTask*)private_data;
    if (!task) return;
    g_memory->dealloc(g_memory, task);
}

static void interval_task_cleanup(void* private_data) {
    IntervalTask* task = (IntervalTask*)private_data;
    if (!task) return;
    g_memory->dealloc(g_memory, task);
}

static void parallel_sequence_task_cleanup(void* private_data) {
    ParallelSequenceData* data = (ParallelSequenceData*)private_data;
    if (!data) return;
    if (data->tasks) {
        g_memory->dealloc(g_memory, data->tasks);
    }
    g_memory->dealloc(g_memory, data);
}

// 异步操作结构体
bool init_memory() {
    if (g_memory) return true;
    
    InfraxMemoryConfig config = {
        .initial_size = 1024 * 1024,  // 1MB
        .use_gc = false,
        .use_pool = true,
        .gc_threshold = 0
    };
    
    g_memory = InfraxMemoryClass.new(&config);
    g_core = InfraxCoreClass.singleton();
    return g_memory != NULL && g_core != NULL;
}

// 构造函数
PolyxAsync* polyx_async_new(void) {
    if (!init_memory()) return NULL;
    
    PolyxAsync* self = g_memory->alloc(g_memory, sizeof(PolyxAsync));
    if (!self) return NULL;
    
    // Initialize result
    self->result = g_memory->alloc(g_memory, sizeof(PolyxAsyncResult));
    if (!self->result) {
        g_memory->dealloc(g_memory, self);
        return NULL;
    }
    
    self->result->data = NULL;
    self->result->size = 0;
    self->result->error_code = 0;
    self->result->status = POLYX_ASYNC_PENDING;
    
    // Initialize other members
    self->infra = NULL;
    self->private_data = NULL;
    self->callback = NULL;
    
    // Initialize instance methods
    self->start = polyx_async_start;
    self->cancel = polyx_async_cancel;
    self->is_done = polyx_async_is_done;
    self->get_result = polyx_async_get_result;
    self->free = polyx_async_free;
    
    return self;
}

// 析构函数
void polyx_async_free(PolyxAsync* self) {
    if (!self) return;
    
    // 先清理私有数据
    if (self->private_data && self->cleanup_fn) {
        self->cleanup_fn(self->private_data);
    }
    
    if (self->infra) {
        infrax_async_free(self->infra);
    }
    
    if (self->result) {
        if (self->result->data) {
            g_memory->dealloc(g_memory, self->result->data);
        }
        g_memory->dealloc(g_memory, self->result);
    }
    
    g_memory->dealloc(g_memory, self);
}

// 实现实例方法
PolyxAsync* polyx_async_start(PolyxAsync* self) {
    if (!self || !self->infra) return self;
    infrax_async_start(self->infra);
    return self;
}

void polyx_async_cancel(PolyxAsync* self) {
    if (!self || !self->infra) return;
    self->infra->error = ECANCELED;
    self->infra->state = INFRAX_ASYNC_REJECTED;
}

bool polyx_async_is_done(PolyxAsync* self) {
    if (!self || !self->infra) return false;
    return self->infra->state == INFRAX_ASYNC_FULFILLED || 
           self->infra->state == INFRAX_ASYNC_REJECTED;
}

void* polyx_async_get_result(PolyxAsync* self, size_t* size) {
    if (!self || !self->infra) return NULL;
    
    // Create result if not exists
    if (!self->result) {
        self->result = (PolyxAsyncResult*)malloc(sizeof(PolyxAsyncResult));
        if (!self->result) return NULL;
        
        self->result->data = NULL;
        self->result->size = 0;
        self->result->error_code = 0;
    }
    
    // Update result
    if (self->infra->state == INFRAX_ASYNC_REJECTED) {
        self->result->error_code = self->infra->error;
        if (size) *size = 0;
        return NULL;
    }
    
    // Get result data
    void* data = InfraxAsyncClass.get_result(self->infra, size);
    if (data) {
        self->result->data = data;
        self->result->size = *size;
    }
    
    return data;
}

// 文件读取操作
PolyxAsync* polyx_async_read_file(const char* path) {
    if (!path) return NULL;
    
    PolyxAsync* self = polyx_async_new();
    if (!self) return NULL;
    
    FileReadTask* task = g_memory->alloc(g_memory, sizeof(FileReadTask));
    if (!task) {
        polyx_async_free(self);
        return NULL;
    }
    
    size_t path_len = strlen(path) + 1;
    task->path = g_memory->alloc(g_memory, path_len);
    if (!task->path) {
        g_memory->dealloc(g_memory, task);
        polyx_async_free(self);
        return NULL;
    }
    memcpy(task->path, path, path_len);
    
    self->infra = infrax_async_new(async_read_file_fn, task);
    if (!self->infra) {
        file_read_task_cleanup(task);
        polyx_async_free(self);
        return NULL;
    }
    
    self->private_data = task;
    self->cleanup_fn = file_read_task_cleanup;
    
    return self;
}

// 文件写入操作
PolyxAsync* polyx_async_write_file(const char* path, const void* data, size_t size) {
    if (!path || !data) return NULL;
    
    PolyxAsync* self = polyx_async_new();
    if (!self) return NULL;
    
    FileWriteTask* task = g_memory->alloc(g_memory, sizeof(FileWriteTask));
    if (!task) {
        polyx_async_free(self);
        return NULL;
    }
    
    size_t path_len = strlen(path) + 1;
    task->path = g_memory->alloc(g_memory, path_len);
    if (!task->path) {
        g_memory->dealloc(g_memory, task);
        polyx_async_free(self);
        return NULL;
    }
    memcpy(task->path, path, path_len);
    
    task->data = g_memory->alloc(g_memory, size);
    if (!task->data) {
        file_write_task_cleanup(task);
        polyx_async_free(self);
        return NULL;
    }
    memcpy(task->data, data, size);
    task->size = size;
    
    self->infra = infrax_async_new(async_write_file_fn, task);
    if (!self->infra) {
        file_write_task_cleanup(task);
        polyx_async_free(self);
        return NULL;
    }
    
    self->private_data = task;
    self->cleanup_fn = file_write_task_cleanup;
    
    return self;
}

// 实现延迟操作
PolyxAsync* polyx_async_delay(int ms) {
    if (ms < 0) return NULL;
    
    PolyxAsync* self = polyx_async_new();
    if (!self) return NULL;
    
    DelayTask* task = g_memory->alloc(g_memory, sizeof(DelayTask));
    if (!task) {
        polyx_async_free(self);
        return NULL;
    }
    
    task->ms = ms;
    self->infra = infrax_async_new(async_delay_fn, task);
    if (!self->infra) {
        g_memory->dealloc(g_memory, task);
        polyx_async_free(self);
        return NULL;
    }
    
    self->private_data = task;
    self->cleanup_fn = delay_task_cleanup;
    
    return self;
}

// 并行执行操作
PolyxAsync* polyx_async_parallel(PolyxAsync** tasks, int count) {
    PolyxAsync* self = polyx_async_new();
    if (!self) return NULL;
    
    ParallelSequenceData* parallel_data = g_memory->alloc(g_memory, sizeof(ParallelSequenceData));
    if (!parallel_data) {
        polyx_async_free(self);
        return NULL;
    }
    
    parallel_data->tasks = g_memory->alloc(g_memory, sizeof(PolyxAsync*) * count);
    if (!parallel_data->tasks) {
        g_memory->dealloc(g_memory, parallel_data);
        polyx_async_free(self);
        return NULL;
    }
    
    for (int i = 0; i < count; i++) {
        parallel_data->tasks[i] = tasks[i];
    }
    parallel_data->count = count;
    parallel_data->completed = 0;
    parallel_data->current = 0;
    
    self->private_data = parallel_data;
    self->cleanup_fn = parallel_sequence_task_cleanup;
    
    return self;
}

// 序列执行操作
PolyxAsync* polyx_async_sequence(PolyxAsync** tasks, int count) {
    PolyxAsync* self = polyx_async_new();
    if (!self) return NULL;
    
    // Initialize result
    self->result = g_memory->alloc(g_memory, sizeof(PolyxAsyncResult));
    if (!self->result) {
        polyx_async_free(self);
        return NULL;
    }
    self->result->data = NULL;
    self->result->size = 0;
    self->result->error_code = 0;
    
    // Store tasks for later use
    self->private_data = g_memory->alloc(g_memory, sizeof(ParallelSequenceData));
    
    if (!self->private_data) {
        g_memory->dealloc(g_memory, self->result);
        polyx_async_free(self);
        return NULL;
    }
    
    ParallelSequenceData* sequence_data = self->private_data;
    
    sequence_data->tasks = g_memory->alloc(g_memory, sizeof(PolyxAsync*) * count);
    if (!sequence_data->tasks) {
        g_memory->dealloc(g_memory, self->private_data);
        g_memory->dealloc(g_memory, self->result);
        polyx_async_free(self);
        return NULL;
    }
    
    for (int i = 0; i < count; i++) {
        sequence_data->tasks[i] = tasks[i];
    }
    sequence_data->count = count;
    sequence_data->completed = 0;
    sequence_data->current = 0;
    
    // Set up instance methods
    self->start = polyx_async_sequence_start;
    self->cancel = polyx_async_sequence_cancel;
    self->is_done = polyx_async_sequence_is_done;
    self->get_result = polyx_async_sequence_get_result;
    
    return self;
}

// HTTP GET操作
PolyxAsync* polyx_async_http_get(const char* url) {
    if (!url) return NULL;
    
    PolyxAsync* self = polyx_async_new();
    if (!self) return NULL;
    
    // 创建HTTP GET任务
    HttpGetTask* task = g_memory->alloc(g_memory, sizeof(HttpGetTask));
    if (!task) {
        polyx_async_free(self);
        return NULL;
    }
    
    // 复制URL
    size_t url_len = strlen(url) + 1;
    task->url = g_memory->alloc(g_memory, url_len);
    if (!task->url) {
        g_memory->dealloc(g_memory, task);
        polyx_async_free(self);
        return NULL;
    }
    memcpy(task->url, url, url_len);
    
    // 创建底层异步任务
    self->infra = infrax_async_new(async_http_get_fn, task);
    if (!self->infra) {
        g_memory->dealloc(g_memory, task->url);
        g_memory->dealloc(g_memory, task);
        polyx_async_free(self);
        return NULL;
    }
    
    self->private_data = task;
    self->cleanup_fn = http_get_task_cleanup;
    
    return self;
}

// HTTP POST操作
PolyxAsync* polyx_async_http_post(const char* url, const void* data, size_t size) {
    if (!url || !data) return NULL;
    
    PolyxAsync* self = polyx_async_new();
    if (!self) return NULL;
    
    // 创建HTTP POST任务
    HttpPostTask* task = g_memory->alloc(g_memory, sizeof(HttpPostTask));
    if (!task) {
        polyx_async_free(self);
        return NULL;
    }
    
    // 复制URL
    size_t url_len = strlen(url) + 1;
    task->url = g_memory->alloc(g_memory, url_len);
    if (!task->url) {
        g_memory->dealloc(g_memory, task);
        polyx_async_free(self);
        return NULL;
    }
    memcpy(task->url, url, url_len);
    
    // 复制数据
    task->data = g_memory->alloc(g_memory, size);
    if (!task->data) {
        g_memory->dealloc(g_memory, task->url);
        g_memory->dealloc(g_memory, task);
        polyx_async_free(self);
        return NULL;
    }
    memcpy(task->data, data, size);
    task->size = size;
    
    // 创建底层异步任务
    self->infra = infrax_async_new(async_http_post_fn, task);
    if (!self->infra) {
        g_memory->dealloc(g_memory, task->data);
        g_memory->dealloc(g_memory, task->url);
        g_memory->dealloc(g_memory, task);
        polyx_async_free(self);
        return NULL;
    }
    
    self->private_data = task;
    self->cleanup_fn = http_post_task_cleanup;
    
    return self;
}

// 间隔执行操作
PolyxAsync* polyx_async_interval(int ms, int count) {
    if (ms < 0 || count < 0) return NULL;
    
    PolyxAsync* self = polyx_async_new();
    if (!self) return NULL;
    
    // 创建间隔任务数据
    IntervalTask* task = g_memory->alloc(g_memory, sizeof(IntervalTask));
    if (!task) {
        polyx_async_free(self);
        return NULL;
    }
    
    task->ms = ms;
    task->count = count;
    task->current = 0;
    
    // 创建底层异步任务
    self->infra = infrax_async_new(async_interval_fn, task);
    if (!self->infra) {
        g_memory->dealloc(g_memory, task);
        polyx_async_free(self);
        return NULL;
    }
    
    return self;
}

// 并行执行操作的实例方法
PolyxAsync* polyx_async_parallel_start(PolyxAsync* self) {
    if (!self || !self->private_data) return self;
    
    ParallelSequenceData* parallel_data = self->private_data;
    
    // Start all tasks
    for (int i = 0; i < parallel_data->count; i++) {
        if (parallel_data->tasks[i]) {
            parallel_data->tasks[i] = parallel_data->tasks[i]->start(parallel_data->tasks[i]);
        }
    }
    
    return self;
}

bool polyx_async_parallel_is_done(PolyxAsync* self) {
    if (!self || !self->private_data) return true;
    
    ParallelSequenceData* parallel_data = self->private_data;
    
    // Check if all tasks are done
    parallel_data->completed = 0;
    for (int i = 0; i < parallel_data->count; i++) {
        if (parallel_data->tasks[i] && parallel_data->tasks[i]->is_done(parallel_data->tasks[i])) {
            parallel_data->completed++;
        }
    }
    
    return parallel_data->completed == parallel_data->count;
}

void* polyx_async_parallel_get_result(PolyxAsync* self, size_t* size) {
    if (!self || !self->infra) {
        if (size) *size = 0;
        return NULL;
    }
    
    ParallelSequenceData* parallel_data = (ParallelSequenceData*)self->infra->ctx->user_data;
    if (!parallel_data) {
        if (size) *size = 0;
        return NULL;
    }
    
    // Create result if not exists
    if (!self->result) {
        self->result = g_memory->alloc(g_memory, sizeof(PolyxAsyncResult));
        if (!self->result) {
            if (size) *size = 0;
            return NULL;
        }
        
        self->result->data = NULL;
        self->result->size = 0;
        self->result->error_code = 0;
    }
    
    // Check if any task failed
    for (size_t i = 0; i < parallel_data->count; i++) {
        if (parallel_data->tasks[i]->infra->state == INFRAX_ASYNC_REJECTED) {
            self->result->error_code = parallel_data->tasks[i]->infra->error;
            if (size) *size = 0;
            return NULL;
        }
    }
    
    // Get result from last task
    if (parallel_data->count > 0) {
        size_t task_size;
        void* data = parallel_data->tasks[parallel_data->count - 1]->get_result(
            parallel_data->tasks[parallel_data->count - 1], &task_size);
            
        self->result->data = data;
        self->result->size = task_size;
        if (size) *size = task_size;
        return data;
    }
    
    if (size) *size = 0;
    return NULL;
}

void polyx_async_parallel_cancel(PolyxAsync* self) {
    if (!self || !self->private_data) return;
    
    ParallelSequenceData* parallel_data = self->private_data;
    
    // Cancel all tasks
    for (int i = 0; i < parallel_data->count; i++) {
        if (parallel_data->tasks[i]) {
            parallel_data->tasks[i]->cancel(parallel_data->tasks[i]);
        }
    }
}

// 序列执行操作的实例方法
PolyxAsync* polyx_async_sequence_start(PolyxAsync* self) {
    if (!self || !self->private_data) return self;
    
    ParallelSequenceData* sequence_data = self->private_data;
    
    // Start first task if not started
    if (sequence_data->current < sequence_data->count && sequence_data->tasks[sequence_data->current]) {
        sequence_data->tasks[sequence_data->current] = 
            sequence_data->tasks[sequence_data->current]->start(sequence_data->tasks[sequence_data->current]);
    }
    
    return self;
}

bool polyx_async_sequence_is_done(PolyxAsync* self) {
    if (!self || !self->private_data) return true;
    
    ParallelSequenceData* sequence_data = self->private_data;
    
    // Check current task
    if (sequence_data->current >= sequence_data->count) {
        return true;
    }
    
    if (!sequence_data->tasks[sequence_data->current]) {
        sequence_data->current++;
        return sequence_data->current >= sequence_data->count;
    }
    
    if (sequence_data->tasks[sequence_data->current]->is_done(sequence_data->tasks[sequence_data->current])) {
        // Start next task
        sequence_data->current++;
        if (sequence_data->current < sequence_data->count && sequence_data->tasks[sequence_data->current]) {
            sequence_data->tasks[sequence_data->current] = 
                sequence_data->tasks[sequence_data->current]->start(sequence_data->tasks[sequence_data->current]);
        }
    }
    
    return sequence_data->current >= sequence_data->count;
}

void* polyx_async_sequence_get_result(PolyxAsync* self, size_t* size) {
    if (!self || !self->infra) {
        if (size) *size = 0;
        return NULL;
    }
    
    ParallelSequenceData* sequence_data = (ParallelSequenceData*)self->infra->ctx->user_data;
    if (!sequence_data) {
        if (size) *size = 0;
        return NULL;
    }
    
    // Create result if not exists
    if (!self->result) {
        self->result = g_memory->alloc(g_memory, sizeof(PolyxAsyncResult));
        if (!self->result) {
            if (size) *size = 0;
            return NULL;
        }
        
        self->result->data = NULL;
        self->result->size = 0;
        self->result->error_code = 0;
    }
    
    // Get result from last completed task
    if (sequence_data->count > 0) {
        PolyxAsync* last_task = sequence_data->tasks[sequence_data->count - 1];
        if (last_task && last_task->is_done(last_task)) {
            size_t task_size;
            void* data = last_task->get_result(last_task, &task_size);
            
            self->result->data = data;
            self->result->size = task_size;
            if (size) *size = task_size;
            return data;
        }
    }
    
    if (size) *size = 0;
    return NULL;
}

void polyx_async_sequence_cancel(PolyxAsync* self) {
    if (!self || !self->private_data) return;
    
    ParallelSequenceData* sequence_data = self->private_data;
    
    // Cancel current task
    if (sequence_data->current < sequence_data->count && sequence_data->tasks[sequence_data->current]) {
        sequence_data->tasks[sequence_data->current]->cancel(sequence_data->tasks[sequence_data->current]);
    }
}

// 全局类实例定义
const PolyxAsyncClassType PolyxAsyncClass = {
    .new = polyx_async_new,
    .free = polyx_async_free,
    .read_file = polyx_async_read_file,
    .write_file = polyx_async_write_file,
    .http_get = polyx_async_http_get,
    .http_post = polyx_async_http_post,
    .delay = polyx_async_delay,
    .interval = polyx_async_interval,
    .parallel = polyx_async_parallel,
    .sequence = polyx_async_sequence
};

// 异步间隔执行回调
void async_interval_fn(InfraxAsync* async, void* arg) {
    if (!async || !arg) return;
    
    IntervalTask* task = (IntervalTask*)arg;
    
    while (task->current < task->count && async->state != INFRAX_ASYNC_REJECTED) {
        // 使用 Core 的 sleep_ms 函数
        g_core->sleep_ms(g_core, task->ms);
        
        task->current++;
        
        // 每次间隔后让出 CPU
        g_core->yield(g_core);
    }
    
    async->state = INFRAX_ASYNC_FULFILLED;
}
