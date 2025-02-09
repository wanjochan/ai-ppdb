#include "internal/polyx/PolyxAsync.h"
#include "internal/infrax/InfraxMemory.h"
#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxNet.h"
#include <time.h>
#include <errno.h>

// 全局内存管理器
static InfraxMemory* g_memory = NULL;
extern const InfraxMemoryClassType InfraxMemoryClass;

// 内部函数声明
void async_read_file_fn(InfraxAsync* async, void* arg);
void async_write_file_fn(InfraxAsync* async, void* arg);
void async_http_get_fn(InfraxAsync* async, void* arg);
void async_http_post_fn(InfraxAsync* async, void* arg);
void async_delay_fn(InfraxAsync* async, void* arg);
void async_interval_fn(InfraxAsync* async, void* arg);
void async_parallel_fn(InfraxAsync* async, void* arg);
void async_sequence_fn(InfraxAsync* async, void* arg);

// 实例方法声明
PolyxAsync* polyx_async_start(PolyxAsync* self);
void polyx_async_cancel(PolyxAsync* self);
bool polyx_async_is_done(PolyxAsync* self);
PolyxAsyncResult* polyx_async_get_result(PolyxAsync* self);

// 并行和序列执行方法声明
PolyxAsync* polyx_async_parallel_start(PolyxAsync* self);
void polyx_async_parallel_cancel(PolyxAsync* self);
bool polyx_async_parallel_is_done(PolyxAsync* self);
PolyxAsyncResult* polyx_async_parallel_get_result(PolyxAsync* self);

PolyxAsync* polyx_async_sequence_start(PolyxAsync* self);
void polyx_async_sequence_cancel(PolyxAsync* self);
bool polyx_async_sequence_is_done(PolyxAsync* self);
PolyxAsyncResult* polyx_async_sequence_get_result(PolyxAsync* self);

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
    return g_memory != NULL;
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
    
    if (self->infra) {
        infrax_async_free(self->infra);
    }
    
    if (self->result) {
        if (self->result->data) {
            g_memory->dealloc(g_memory, self->result->data);
        }
        g_memory->dealloc(g_memory, self->result);
    }
    
    if (self->private_data) {
        g_memory->dealloc(g_memory, self->private_data);
    }
    
    g_memory->dealloc(g_memory, self);
}

// 实现实例方法
PolyxAsync* polyx_async_start(PolyxAsync* self) {
    if (!self || !self->infra) return self;
    self->infra->start(self->infra, self->infra->fn, self->infra->arg);
    return self;
}

void polyx_async_cancel(PolyxAsync* self) {
    if (!self || !self->infra) return;
    // 将状态设置为错误状态来模拟取消
    self->infra->state = INFRAX_ASYNC_REJECTED;
    self->infra->error = ECANCELED;
}

bool polyx_async_is_done(PolyxAsync* self) {
    if (!self || !self->infra) return false;
    return self->infra->state == INFRAX_ASYNC_FULFILLED || 
           self->infra->state == INFRAX_ASYNC_REJECTED;
}

PolyxAsyncResult* polyx_async_get_result(PolyxAsync* self) {
    if (!self || !self->infra) return NULL;
    
    if (!self->result) {
        self->result = (PolyxAsyncResult*)g_memory->alloc(g_memory, sizeof(PolyxAsyncResult));
        if (!self->result) return NULL;
        
        self->result->data = NULL;
        self->result->size = 0;
        self->result->error_code = 0;
    }
    
    // 更新结果
    if (self->infra->state == INFRAX_ASYNC_REJECTED) {
        self->result->error_code = self->infra->error;
    } else if (self->infra->state == INFRAX_ASYNC_FULFILLED && self->infra->result) {
        // 根据实际操作类型设置结果
        self->result->data = self->infra->result;
        // TODO: 根据操作类型设置 size
    }
    
    return self->result;
}

// 文件读取操作
PolyxAsync* polyx_async_read_file(const char* path) {
    if (!path) return NULL;
    
    PolyxAsync* self = polyx_async_new();
    if (!self) return NULL;
    
    // 创建文件读取任务
    FileReadTask* task = g_memory->alloc(g_memory, sizeof(FileReadTask));
    if (!task) {
        polyx_async_free(self);
        return NULL;
    }
    
    // 复制文件路径
    size_t path_len = strlen(path) + 1;
    task->path = g_memory->alloc(g_memory, path_len);
    if (!task->path) {
        g_memory->dealloc(g_memory, task);
        polyx_async_free(self);
        return NULL;
    }
    memcpy(task->path, path, path_len);
    
    // 创建底层异步任务
    self->infra = infrax_async_new(async_read_file_fn, task);
    if (!self->infra) {
        g_memory->dealloc(g_memory, task->path);
        g_memory->dealloc(g_memory, task);
        polyx_async_free(self);
        return NULL;
    }
    
    return self;
}

// 文件写入操作
PolyxAsync* polyx_async_write_file(const char* path, const void* data, size_t size) {
    if (!path || !data) return NULL;
    
    PolyxAsync* self = polyx_async_new();
    if (!self) return NULL;
    
    // 创建文件写入任务
    FileWriteTask* task = g_memory->alloc(g_memory, sizeof(FileWriteTask));
    if (!task) {
        polyx_async_free(self);
        return NULL;
    }
    
    // 复制文件路径
    size_t path_len = strlen(path) + 1;
    task->path = g_memory->alloc(g_memory, path_len);
    if (!task->path) {
        g_memory->dealloc(g_memory, task);
        polyx_async_free(self);
        return NULL;
    }
    memcpy(task->path, path, path_len);
    
    // 复制数据
    task->data = g_memory->alloc(g_memory, size);
    if (!task->data) {
        g_memory->dealloc(g_memory, task->path);
        g_memory->dealloc(g_memory, task);
        polyx_async_free(self);
        return NULL;
    }
    memcpy(task->data, data, size);
    task->size = size;
    
    // 创建底层异步任务
    self->infra = infrax_async_new(async_write_file_fn, task);
    if (!self->infra) {
        g_memory->dealloc(g_memory, task->data);
        g_memory->dealloc(g_memory, task->path);
        g_memory->dealloc(g_memory, task);
        polyx_async_free(self);
        return NULL;
    }
    
    return self;
}

// 实现延迟操作
PolyxAsync* polyx_async_delay(int ms) {
    if (ms < 0) return NULL;
    
    PolyxAsync* self = polyx_async_new();
    if (!self) return NULL;
    
    int* delay_ms = (int*)g_memory->alloc(g_memory, sizeof(int));
    if (!delay_ms) {
        polyx_async_free(self);
        return NULL;
    }
    
    *delay_ms = ms;
    self->infra = infrax_async_new(async_delay_fn, delay_ms);
    if (!self->infra) {
        g_memory->dealloc(g_memory, delay_ms);
        polyx_async_free(self);
        return NULL;
    }
    
    return self;
}

// 并行执行操作
PolyxAsync* polyx_async_parallel(PolyxAsync** tasks, int count) {
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
    
    ParallelSequenceData* parallel_data = self->private_data;
    
    parallel_data->tasks = g_memory->alloc(g_memory, sizeof(PolyxAsync*) * count);
    if (!parallel_data->tasks) {
        g_memory->dealloc(g_memory, self->private_data);
        g_memory->dealloc(g_memory, self->result);
        polyx_async_free(self);
        return NULL;
    }
    
    for (int i = 0; i < count; i++) {
        parallel_data->tasks[i] = tasks[i];
    }
    parallel_data->count = count;
    parallel_data->completed = 0;
    parallel_data->current = 0;
    
    // Set up instance methods
    self->start = polyx_async_parallel_start;
    self->cancel = polyx_async_parallel_cancel;
    self->is_done = polyx_async_parallel_is_done;
    self->get_result = polyx_async_parallel_get_result;
    
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
    
    return self;
}

// 异步文件读取回调
void async_read_file_fn(InfraxAsync* async, void* arg) {
    FileReadTask* task = (FileReadTask*)arg;
    if (!task || !task->path) {
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = EINVAL;
        return;
    }

    FILE* file = fopen(task->path, "rb");
    if (!file) {
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = errno;
        return;
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // 分配内存
    void* buffer = g_memory->alloc(g_memory, file_size);
    if (!buffer) {
        fclose(file);
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = ENOMEM;
        return;
    }

    // 读取文件内容
    size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != (size_t)file_size) {
        fclose(file);
        g_memory->dealloc(g_memory, buffer);
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = ferror(file) ? errno : EIO;
        return;
    }

    fclose(file);
    async->result = buffer;
    async->state = INFRAX_ASYNC_FULFILLED;
}

// 异步文件写入回调
void async_write_file_fn(InfraxAsync* async, void* arg) {
    FileWriteTask* task = (FileWriteTask*)arg;
    if (!task || !task->path || !task->data) {
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = EINVAL;
        return;
    }

    FILE* file = fopen(task->path, "wb");
    if (!file) {
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = errno;
        return;
    }

    // 写入文件内容
    size_t bytes_written = fwrite(task->data, 1, task->size, file);
    if (bytes_written != task->size) {
        fclose(file);
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = ferror(file) ? errno : EIO;
        return;
    }

    fclose(file);
    async->state = INFRAX_ASYNC_FULFILLED;
}

// 异步延时回调
void async_delay_fn(InfraxAsync* async, void* arg) {
    int* ms = (int*)arg;
    if (!ms) {
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = EINVAL;
        return;
    }

    struct timespec ts = {
        .tv_sec = *ms / 1000,
        .tv_nsec = (*ms % 1000) * 1000000
    };
    
    if (nanosleep(&ts, NULL) != 0) {
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = errno;
        return;
    }

    async->state = INFRAX_ASYNC_FULFILLED;
}

// 异步并行执行回调
void async_parallel_fn(InfraxAsync* async, void* arg) {
    ParallelSequenceData* data = (ParallelSequenceData*)arg;
    if (!data || !data->tasks || data->count <= 0) {
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = EINVAL;
        return;
    }

    // 启动所有任务
    for (int i = 0; i < data->count; i++) {
        if (data->tasks[i]) {
            data->tasks[i]->start(data->tasks[i]);
        }
    }

    // 检查所有任务的完成状态
    while (true) {
        bool all_done = true;
        bool any_error = false;

        for (int i = 0; i < data->count; i++) {
            if (!data->tasks[i]) continue;

            if (!data->tasks[i]->is_done(data->tasks[i])) {
                all_done = false;
                async->yield(async);
                break;
            }

            PolyxAsyncResult* result = data->tasks[i]->get_result(data->tasks[i]);
            if (result && result->error_code != 0) {
                any_error = true;
                async->error = result->error_code;
                break;
            }
        }

        if (any_error) {
            async->state = INFRAX_ASYNC_REJECTED;
            return;
        }

        if (all_done) {
            async->state = INFRAX_ASYNC_FULFILLED;
            return;
        }
    }
}

// 异步序列执行回调
void async_sequence_fn(InfraxAsync* async, void* arg) {
    ParallelSequenceData* data = (ParallelSequenceData*)arg;
    if (!data || !data->tasks || data->count <= 0) {
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = EINVAL;
        return;
    }

    // 按顺序执行任务
    for (int i = 0; i < data->count; i++) {
        if (!data->tasks[i]) continue;

        // 启动当前任务
        data->tasks[i]->start(data->tasks[i]);

        // 等待任务完成
        while (!data->tasks[i]->is_done(data->tasks[i])) {
            async->yield(async);
        }

        // 检查任务结果
        PolyxAsyncResult* result = data->tasks[i]->get_result(data->tasks[i]);
        if (result && result->error_code != 0) {
            async->state = INFRAX_ASYNC_REJECTED;
            async->error = result->error_code;
            return;
        }
    }

    async->state = INFRAX_ASYNC_FULFILLED;
}

// 异步HTTP GET回调
void async_http_get_fn(InfraxAsync* async, void* arg) {
    HttpGetTask* task = (HttpGetTask*)arg;
    if (!task || !task->url) {
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = EINVAL;
        return;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = errno;
        return;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = errno;
        return;
    }

    char request[1024];
    snprintf(request, sizeof(request), "GET %s HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n", task->url);

    if (send(sock, request, strlen(request), 0) < 0) {
        close(sock);
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = errno;
        return;
    }

    char response[1024];
    int bytes_received = recv(sock, response, sizeof(response), 0);
    if (bytes_received < 0) {
        close(sock);
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = errno;
        return;
    }

    async->result = g_memory->alloc(g_memory, bytes_received);
    if (!async->result) {
        close(sock);
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = ENOMEM;
        return;
    }

    memcpy(async->result, response, bytes_received);
    close(sock);
    async->state = INFRAX_ASYNC_FULFILLED;
}

// 异步HTTP POST回调
void async_http_post_fn(InfraxAsync* async, void* arg) {
    HttpPostTask* task = (HttpPostTask*)arg;
    if (!task || !task->url || !task->data) {
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = EINVAL;
        return;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = errno;
        return;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(80);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(sock);
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = errno;
        return;
    }

    char request[1024];
    snprintf(request, sizeof(request), "POST %s HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: %zu\r\n\r\n", task->url, task->size);

    if (send(sock, request, strlen(request), 0) < 0) {
        close(sock);
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = errno;
        return;
    }

    if (send(sock, task->data, task->size, 0) < 0) {
        close(sock);
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = errno;
        return;
    }

    char response[1024];
    int bytes_received = recv(sock, response, sizeof(response), 0);
    if (bytes_received < 0) {
        close(sock);
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = errno;
        return;
    }

    async->result = g_memory->alloc(g_memory, bytes_received);
    if (!async->result) {
        close(sock);
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = ENOMEM;
        return;
    }

    memcpy(async->result, response, bytes_received);
    close(sock);
    async->state = INFRAX_ASYNC_FULFILLED;
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

// 异步间隔执行回调
void async_interval_fn(InfraxAsync* async, void* arg) {
    IntervalTask* task = (IntervalTask*)arg;
    if (!task) {
        async->state = INFRAX_ASYNC_REJECTED;
        async->error = EINVAL;
        return;
    }

    while (task->current < task->count) {
        struct timespec ts = {
            .tv_sec = task->ms / 1000,
            .tv_nsec = (task->ms % 1000) * 1000000
        };
        
        if (nanosleep(&ts, NULL) != 0) {
            async->state = INFRAX_ASYNC_REJECTED;
            async->error = errno;
            return;
        }

        task->current++;
        async->yield(async);
    }

    async->state = INFRAX_ASYNC_FULFILLED;
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

PolyxAsyncResult* polyx_async_parallel_get_result(PolyxAsync* self) {
    if (!self || !self->private_data) return NULL;
    
    ParallelSequenceData* parallel_data = self->private_data;
    
    // Combine results if needed
    for (int i = 0; i < parallel_data->count; i++) {
        if (parallel_data->tasks[i]) {
            PolyxAsyncResult* task_result = parallel_data->tasks[i]->get_result(parallel_data->tasks[i]);
            if (task_result && task_result->error_code != 0) {
                self->result->error_code = task_result->error_code;
                break;
            }
        }
    }
    
    return self->result;
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

PolyxAsyncResult* polyx_async_sequence_get_result(PolyxAsync* self) {
    if (!self || !self->private_data) return NULL;
    
    ParallelSequenceData* sequence_data = self->private_data;
    
    // Return result of last task
    if (sequence_data->count > 0 && sequence_data->tasks[sequence_data->count - 1]) {
        PolyxAsyncResult* last_result = 
            sequence_data->tasks[sequence_data->count - 1]->get_result(sequence_data->tasks[sequence_data->count - 1]);
        if (last_result) {
            self->result->data = last_result->data;
            self->result->size = last_result->size;
            self->result->error_code = last_result->error_code;
        }
    }
    
    return self->result;
}

void polyx_async_sequence_cancel(PolyxAsync* self) {
    if (!self || !self->private_data) return;
    
    ParallelSequenceData* sequence_data = self->private_data;
    
    // Cancel current task
    if (sequence_data->current < sequence_data->count && sequence_data->tasks[sequence_data->current]) {
        sequence_data->tasks[sequence_data->current]->cancel(sequence_data->tasks[sequence_data->current]);
    }
}

// 添加缺失的函数声明
void infrax_async_start(InfraxAsync* async);
void infrax_async_cancel(InfraxAsync* async);
bool infrax_async_is_done(InfraxAsync* async);

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
