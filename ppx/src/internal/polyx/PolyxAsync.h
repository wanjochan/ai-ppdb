#ifndef POLYX_ASYNC_H
#define POLYX_ASYNC_H
/**
poll() 基本原理
poll() 是一个多路复用 I/O 机制
可以同时监控多个文件描述符的状态变化
主要监控读、写、异常三种事件
可被 poll() 监控的主要 fd 类型:
a) 网络相关
TCP sockets
UDP sockets
Unix domain sockets
b) 标准 I/O
管道(pipes)
FIFO
终端设备(/dev/tty)
标准输入输出(stdin/stdout/stderr)
c) 其他
字符设备
事件fd (eventfd)
定时器fd (timerfd)
信号fd (signalfd)
inotify fd (文件系统事件监控)
 */
#include "internal/infrax/InfraxAsync.h"
#include "internal/infrax/InfraxCore.h"
#include "internal/infrax/InfraxMemory.h"
// #include <poll.h>

// Forward declarations
typedef struct PolyxAsync PolyxAsync;
typedef struct PolyxEvent PolyxEvent;
typedef struct PolyxAsyncClassType PolyxAsyncClassType;

// Event types
typedef enum {
    POLYX_EVENT_NONE = 0,
    POLYX_EVENT_IO,
    POLYX_EVENT_TIMER,
    POLYX_EVENT_SIGNAL,
    // Network related
    POLYX_EVENT_TCP,
    POLYX_EVENT_UDP,
    POLYX_EVENT_UNIX,
    // Standard IO
    POLYX_EVENT_PIPE,
    POLYX_EVENT_FIFO,
    POLYX_EVENT_TTY,
    // Others
    POLYX_EVENT_INOTIFY,
    POLYX_EVENT_CHAR_DEV
} PolyxEventType;

// Timer callback type
typedef void (*TimerCallback)(void* arg);

// Event callback type
typedef void (*EventCallback)(PolyxEvent* event, void* arg);

// Timer configuration
typedef struct {
    unsigned int interval_ms;
    TimerCallback callback;
    void* arg;
} PolyxTimerConfig;

// Event configuration
typedef struct {
    PolyxEventType type;
    EventCallback callback;
    void* arg;
} PolyxEventConfig;

// Network event configurations
typedef struct {
    int socket_fd;
    int events;     // POLLIN, POLLOUT etc.
    EventCallback callback;
    void* arg;
} PolyxNetworkConfig;

// IO event configurations
typedef struct {
    int fd;
    int events;     // POLLIN, POLLOUT etc.
    EventCallback callback;
    void* arg;
} PolyxIOConfig;

// File monitor configuration
typedef struct {
    const char* path;
    int watch_mask;  // IN_CREATE, IN_DELETE etc.
    EventCallback callback;
    void* arg;
} PolyxInotifyConfig;

// Event structure
struct PolyxEvent {
    PolyxEventType type;
    union {
        struct {
            int fd;
            int events;
        } io;
        struct {
            int socket_fd;
            int events;
            int protocol;  // IPPROTO_TCP, IPPROTO_UDP etc.
        } network;
        struct {
            int watch_fd;
            char* path;
        } inotify;
        struct {
            uint64_t due_time;
            TimerCallback callback;
        } timer;
    } data;
    EventCallback callback;
    void* arg;
    void* private_data;  // Internal use only
};

// Timer heap node structure
typedef struct TimerHeapNode {
    uint64_t due_time;      // When this timer should fire
    TimerCallback callback;
    void* arg;
    struct TimerHeapNode* left;
    struct TimerHeapNode* right;
    struct TimerHeapNode* parent;
} TimerHeapNode;

// Timer heap structure
typedef struct {
    TimerHeapNode* root;
    size_t size;
} TimerHeap;

// Async structure
struct PolyxAsync {
    PolyxAsync* self;
    const PolyxAsyncClassType* klass;

    InfraxAsync* infrax;
    PolyxEvent** events;
    size_t event_count;
    size_t event_capacity;
};

// 类接口
struct PolyxAsyncClassType {
    PolyxAsync* (*new)(void);
    void (*free)(PolyxAsync* self);

    // 对象方法 - 事件管理
    PolyxEvent* (*create_event)(PolyxAsync* self, PolyxEventConfig* config);
    PolyxEvent* (*create_timer)(PolyxAsync* self, PolyxTimerConfig* config);
    void (*destroy_event)(PolyxAsync* self, PolyxEvent* event);
    
    // 对象方法 - 事件触发
    void (*trigger_event)(PolyxAsync* self, PolyxEvent* event, void* data, size_t size);
     
    // 对象方法 - 定时器控制
    void (*start_timer)(PolyxAsync* self, PolyxEvent* timer);
    void (*stop_timer)(PolyxAsync* self, PolyxEvent* timer);
    
    // Network related methods
    PolyxEvent* (*create_tcp_event)(PolyxAsync* self, PolyxNetworkConfig* config);
    PolyxEvent* (*create_udp_event)(PolyxAsync* self, PolyxNetworkConfig* config);
    PolyxEvent* (*create_unix_event)(PolyxAsync* self, PolyxNetworkConfig* config);
    
    // IO related methods
    PolyxEvent* (*create_pipe_event)(PolyxAsync* self, PolyxIOConfig* config);
    PolyxEvent* (*create_fifo_event)(PolyxAsync* self, PolyxIOConfig* config);
    PolyxEvent* (*create_tty_event)(PolyxAsync* self, PolyxIOConfig* config);
    
    // File monitoring methods
    PolyxEvent* (*create_inotify_event)(PolyxAsync* self, PolyxInotifyConfig* config);
    
    // 对象方法 - 轮询
    int (*poll)(PolyxAsync* self, int timeout_ms);
};

// 全局类实例
extern const PolyxAsyncClassType PolyxAsyncClass;

// Helper macros for event handling
#define POLYX_EVENT_IS_NETWORK(e) ((e)->type >= POLYX_EVENT_TCP && (e)->type <= POLYX_EVENT_UNIX)
#define POLYX_EVENT_IS_IO(e) ((e)->type >= POLYX_EVENT_PIPE && (e)->type <= POLYX_EVENT_TTY)
#define POLYX_EVENT_IS_MONITOR(e) ((e)->type == POLYX_EVENT_INOTIFY)

// Helper functions
static inline int polyx_event_get_fd(PolyxEvent* event) {
    if (POLYX_EVENT_IS_NETWORK(event)) {
        return event->data.network.socket_fd;
    } else if (POLYX_EVENT_IS_IO(event)) {
        return event->data.io.fd;
    } else if (POLYX_EVENT_IS_MONITOR(event)) {
        return event->data.inotify.watch_fd;
    }
    return -1;
}

#endif // POLYX_ASYNC_H

