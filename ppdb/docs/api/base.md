# Base层API文档

## 异步IO系统

### IO管理器

#### ppdb_base_io_manager_create
```c
ppdb_error_t ppdb_base_io_manager_create(
    ppdb_base_io_manager_t** mgr,
    size_t queue_size,
    size_t num_threads
);
```
创建IO管理器。
- `mgr`: 输出参数，返回创建的IO管理器
- `queue_size`: IO请求队列大小
- `num_threads`: 工作线程数量
- 返回：成功返回PPDB_OK，失败返回错误码

#### ppdb_base_io_manager_destroy
```c
ppdb_error_t ppdb_base_io_manager_destroy(
    ppdb_base_io_manager_t* mgr
);
```
销毁IO管理器。
- `mgr`: IO管理器
- 返回：成功返回PPDB_OK，失败返回错误码

#### ppdb_base_io_manager_schedule_priority
```c
ppdb_error_t ppdb_base_io_manager_schedule_priority(
    ppdb_base_io_manager_t* mgr,
    ppdb_base_io_func_t func,
    void* arg,
    int priority
);
```
添加带优先级的IO请求。
- `mgr`: IO管理器
- `func`: IO处理函数
- `arg`: 函数参数
- `priority`: 优先级（0-3，0最高）
- 返回：成功返回PPDB_OK，失败返回错误码

#### ppdb_base_io_manager_adjust_threads
```c
ppdb_error_t ppdb_base_io_manager_adjust_threads(
    ppdb_base_io_manager_t* mgr,
    size_t num_threads
);
```
调整工作线程数量。
- `mgr`: IO管理器
- `num_threads`: 新的线程数量
- 返回：成功返回PPDB_OK，失败返回错误码

### 事件系统

#### ppdb_base_event_loop_create
```c
ppdb_error_t ppdb_base_event_loop_create(
    ppdb_base_event_loop_t** loop
);
```
创建事件循环。
- `loop`: 输出参数，返回创建的事件循环
- 返回：成功返回PPDB_OK，失败返回错误码

#### ppdb_base_event_add_filter
```c
ppdb_error_t ppdb_base_event_add_filter(
    ppdb_base_event_loop_t* loop,
    ppdb_base_event_filter_func filter,
    void* user_data
);
```
添加事件过滤器。
- `loop`: 事件循环
- `filter`: 过滤器函数
- `user_data`: 用户数据
- 返回：成功返回PPDB_OK，失败返回错误码

#### ppdb_base_event_handler_create
```c
ppdb_error_t ppdb_base_event_handler_create(
    ppdb_base_event_loop_t* loop,
    ppdb_base_event_handler_t** handler,
    int fd,
    uint32_t events,
    ppdb_base_event_callback_t callback,
    void* user_data
);
```
创建事件处理器。
- `loop`: 事件循环
- `handler`: 输出参数，返回创建的处理器
- `fd`: 文件描述符
- `events`: 关注的事件
- `callback`: 回调函数
- `user_data`: 用户数据
- 返回：成功返回PPDB_OK，失败返回错误码

### 定时器系统

#### ppdb_base_timer_create
```c
ppdb_error_t ppdb_base_timer_create(
    ppdb_base_timer_t** timer,
    uint64_t interval_ms,
    uint32_t flags,
    int priority,
    ppdb_base_timer_callback_t callback,
    void* user_data
);
```
创建定时器。
- `timer`: 输出参数，返回创建的定时器
- `interval_ms`: 定时间隔（毫秒）
- `flags`: 定时器标志
- `priority`: 优先级（0-3，0最高）
- `callback`: 回调函数
- `user_data`: 用户数据
- 返回：成功返回PPDB_OK，失败返回错误码

#### ppdb_base_timer_set_priority
```c
ppdb_error_t ppdb_base_timer_set_priority(
    ppdb_base_timer_t* timer,
    int priority
);
```
设置定时器优先级。
- `timer`: 定时器
- `priority`: 新的优先级
- 返回：成功返回PPDB_OK，失败返回错误码

#### ppdb_base_timer_get_stats
```c
ppdb_error_t ppdb_base_timer_get_stats(
    ppdb_base_timer_t* timer,
    ppdb_base_timer_stats_t* stats
);
```
获取定时器统计信息。
- `timer`: 定时器
- `stats`: 输出参数，返回统计信息
- 返回：成功返回PPDB_OK，失败返回错误码

## 常量定义

### IO优先级
```c
#define PPDB_IO_PRIORITY_HIGH    0
#define PPDB_IO_PRIORITY_NORMAL  1
#define PPDB_IO_PRIORITY_LOW     2
#define PPDB_IO_PRIORITY_IDLE    3
```

### 定时器标志
```c
#define TIMER_FLAG_NONE      0x00
#define TIMER_FLAG_REPEAT    0x01
#define TIMER_FLAG_PRECISE   0x02
#define TIMER_FLAG_COALESCE  0x04
```

### 事件类型
```c
#define PPDB_EVENT_NONE   0x00
#define PPDB_EVENT_READ   0x01
#define PPDB_EVENT_WRITE  0x02
#define PPDB_EVENT_ERROR  0x04
```

## 数据结构

### IO请求
```c
typedef struct ppdb_base_io_request {
    ppdb_base_io_func_t func;     // IO处理函数
    void* arg;                    // 函数参数
    int priority;                 // 优先级
    uint64_t timestamp;           // 时间戳
} ppdb_base_io_request_t;
```

### 定时器统计
```c
typedef struct ppdb_base_timer_stats {
    uint64_t total_ticks;      // 总触发次数
    uint64_t total_elapsed;    // 总运行时间
    uint64_t min_elapsed;      // 最小运行时间
    uint64_t max_elapsed;      // 最大运行时间
    uint64_t avg_elapsed;      // 平均运行时间
    uint64_t drift;           // 时间漂移
} ppdb_base_timer_stats_t;
```

### 事件处理器
```c
typedef struct ppdb_base_event_handler {
    int fd;                    // 文件描述符
    uint32_t events;           // 关注的事件
    void* user_data;          // 用户数据
    ppdb_base_event_callback_t callback;  // 回调函数
} ppdb_base_event_handler_t;
```

## 错误码

```c
#define PPDB_OK                     0
#define PPDB_BASE_ERR_PARAM        -1
#define PPDB_BASE_ERR_MEMORY       -2
#define PPDB_BASE_ERR_IO           -3
#define PPDB_BASE_ERR_TIMEOUT      -4
#define PPDB_BASE_ERR_BUSY         -5
#define PPDB_BASE_ERR_FULL         -6
#define PPDB_BASE_ERR_NOT_FOUND    -7
#define PPDB_BASE_ERR_INVALID_STATE -8
```

## 使用示例

### 异步IO示例
```c
// 创建IO管理器
ppdb_base_io_manager_t* mgr;
ppdb_error_t err = ppdb_base_io_manager_create(&mgr, 1024, 4);
if (err != PPDB_OK) {
    // 错误处理
}

// 定义IO处理函数
void io_handler(void* arg) {
    // 处理IO请求
}

// 添加高优先级IO请求
err = ppdb_base_io_manager_schedule_priority(mgr, io_handler, data, PPDB_IO_PRIORITY_HIGH);
if (err != PPDB_OK) {
    // 错误处理
}

// 调整线程数量
err = ppdb_base_io_manager_adjust_threads(mgr, 8);
if (err != PPDB_OK) {
    // 错误处理
}

// 清理
ppdb_base_io_manager_destroy(mgr);
```

### 事件处理示例
```c
// 创建事件循环
ppdb_base_event_loop_t* loop;
ppdb_error_t err = ppdb_base_event_loop_create(&loop);
if (err != PPDB_OK) {
    // 错误处理
}

// 定义事件过滤器
bool event_filter(ppdb_base_event_handler_t* handler, uint32_t events) {
    // 过滤事件
    return true;
}

// 添加过滤器
err = ppdb_base_event_add_filter(loop, event_filter, NULL);
if (err != PPDB_OK) {
    // 错误处理
}

// 定义事件回调
void event_callback(ppdb_base_event_handler_t* handler, uint32_t events) {
    // 处理事件
}

// 创建事件处理器
ppdb_base_event_handler_t* handler;
err = ppdb_base_event_handler_create(loop, &handler, fd,
    PPDB_EVENT_READ | PPDB_EVENT_WRITE,
    event_callback, NULL);
if (err != PPDB_OK) {
    // 错误处理
}

// 运行事件循环
err = ppdb_base_event_loop_run(loop, 1000);
if (err != PPDB_OK) {
    // 错误处理
}

// 清理
ppdb_base_event_loop_destroy(loop);
```

### 定时器示例
```c
// 创建定时器
ppdb_base_timer_t* timer;
ppdb_error_t err = ppdb_base_timer_create(&timer, 100,
    TIMER_FLAG_REPEAT | TIMER_FLAG_PRECISE,
    TIMER_PRIORITY_HIGH, timer_callback, NULL);
if (err != PPDB_OK) {
    // 错误处理
}

// 启动定时器
err = ppdb_base_timer_start(timer);
if (err != PPDB_OK) {
    // 错误处理
}

// 获取统计信息
ppdb_base_timer_stats_t stats;
err = ppdb_base_timer_get_stats(timer, &stats);
if (err != PPDB_OK) {
    // 错误处理
}

// 调整优先级
err = ppdb_base_timer_set_priority(timer, TIMER_PRIORITY_NORMAL);
if (err != PPDB_OK) {
    // 错误处理
}

// 清理
ppdb_base_timer_destroy(timer);
``` 