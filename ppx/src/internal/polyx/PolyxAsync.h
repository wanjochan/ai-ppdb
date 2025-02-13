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

// Event status
typedef enum {
    POLYX_EVENT_STATUS_INIT = 0,
    POLYX_EVENT_STATUS_READY,
    POLYX_EVENT_STATUS_ACTIVE,
    POLYX_EVENT_STATUS_PAUSED,
    POLYX_EVENT_STATUS_ERROR,
    POLYX_EVENT_STATUS_CLOSED
} PolyxEventStatus;

// Error codes
typedef enum {
    POLYX_OK = 0,
    POLYX_ERROR_INVALID_PARAM = -1,
    POLYX_ERROR_NO_MEMORY = -2,
    POLYX_ERROR_NOT_SUPPORTED = -3,
    POLYX_ERROR_WOULD_BLOCK = -4,
    POLYX_ERROR_IO = -5,
    POLYX_ERROR_NETWORK = -6,
    POLYX_ERROR_TIMEOUT = -7
} PolyxError;

// Refined network configuration
typedef struct {
    int socket_fd;
    int events;     // POLLIN, POLLOUT etc.
    union {
        struct {
            int backlog;
            bool reuse_addr;
        } tcp;
        struct {
            bool broadcast;
            bool multicast;
        } udp;
        struct {
            bool abstract;
            bool pass_credentials;
        } unix;
    } protocol_opts;
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
    PolyxEventStatus status;
    PolyxError last_error;
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

// Event statistics
typedef struct {
    size_t total_events;
    size_t active_events;
    size_t error_events;
    struct {
        size_t network_events;
        size_t io_events;
        size_t timer_events;
        size_t monitor_events;
    } by_type;
    struct {
        uint64_t total_polls;
        uint64_t total_timeouts;
        uint64_t total_errors;
    } poll_stats;
} PolyxEventStats;

// Debug levels
typedef enum {
    POLYX_DEBUG_NONE = 0,
    POLYX_DEBUG_ERROR = 1,
    POLYX_DEBUG_WARN = 2,
    POLYX_DEBUG_INFO = 3,
    POLYX_DEBUG_DEBUG = 4,
    POLYX_DEBUG_TRACE = 5
} PolyxDebugLevel;

// Debug callback
typedef void (*PolyxDebugCallback)(PolyxDebugLevel level, const char* file, int line, const char* func, const char* msg);

// Async structure
struct PolyxAsync {
    PolyxAsync* self;
    const PolyxAsyncClassType* klass;

    InfraxAsync* infrax;
    PolyxEvent** events;
    size_t event_count;
    size_t event_capacity;
    
    // Statistics and debug
    PolyxEventStats stats;
    PolyxDebugLevel debug_level;
    PolyxDebugCallback debug_callback;
    void* debug_context;
    
    // Private data
    void* private_data;
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
    
    // Statistics methods
    void (*get_stats)(PolyxAsync* self, PolyxEventStats* stats);
    void (*reset_stats)(PolyxAsync* self);
    
    // Debug methods
    void (*set_debug_level)(PolyxAsync* self, PolyxDebugLevel level);
    void (*set_debug_callback)(PolyxAsync* self, PolyxDebugCallback callback, void* context);
    
    // Event group methods
    int (*create_event_group)(PolyxAsync* self, PolyxEvent** events, size_t count);
    int (*wait_event_group)(PolyxAsync* self, int group_id, int timeout_ms);
    void (*destroy_event_group)(PolyxAsync* self, int group_id);
    
    // 对象方法 - 轮询
    int (*poll)(PolyxAsync* self, int timeout_ms);
};

// 全局类实例
extern const PolyxAsyncClassType PolyxAsyncClass;

// More precise event type checking macros
#define POLYX_EVENT_TYPE_VALID(t) ((t) > POLYX_EVENT_NONE && (t) <= POLYX_EVENT_CHAR_DEV)
#define POLYX_EVENT_IS_NETWORK(e) ((e) && (e)->type >= POLYX_EVENT_TCP && (e)->type <= POLYX_EVENT_UNIX)
#define POLYX_EVENT_IS_IO(e) ((e) && (e)->type >= POLYX_EVENT_PIPE && (e)->type <= POLYX_EVENT_TTY)
#define POLYX_EVENT_IS_MONITOR(e) ((e) && (e)->type == POLYX_EVENT_INOTIFY)
#define POLYX_EVENT_IS_ACTIVE(e) ((e) && (e)->status == POLYX_EVENT_STATUS_ACTIVE)
#define POLYX_EVENT_HAS_ERROR(e) ((e) && (e)->status == POLYX_EVENT_STATUS_ERROR)

// Helper functions
static inline int polyx_event_get_fd(const PolyxEvent* event) {
    if (!event) return -1;
    
    if (POLYX_EVENT_IS_NETWORK(event)) {
        return event->data.network.socket_fd;
    } else if (POLYX_EVENT_IS_IO(event)) {
        return event->data.io.fd;
    } else if (POLYX_EVENT_IS_MONITOR(event)) {
        return event->data.inotify.watch_fd;
    }
    return -1;
}

static inline bool polyx_event_is_readable(const PolyxEvent* event) {
    if (!POLYX_EVENT_IS_ACTIVE(event)) return false;
    
    if (POLYX_EVENT_IS_NETWORK(event)) {
        return (event->data.network.events & POLLIN) != 0;
    } else if (POLYX_EVENT_IS_IO(event)) {
        return (event->data.io.events & POLLIN) != 0;
    }
    return false;
}

static inline bool polyx_event_is_writable(const PolyxEvent* event) {
    if (!POLYX_EVENT_IS_ACTIVE(event)) return false;
    
    if (POLYX_EVENT_IS_NETWORK(event)) {
        return (event->data.network.events & POLLOUT) != 0;
    } else if (POLYX_EVENT_IS_IO(event)) {
        return (event->data.io.events & POLLOUT) != 0;
    }
    return false;
}

static inline const char* polyx_event_status_str(PolyxEventStatus status) {
    switch (status) {
        case POLYX_EVENT_STATUS_INIT: return "INIT";
        case POLYX_EVENT_STATUS_READY: return "READY";
        case POLYX_EVENT_STATUS_ACTIVE: return "ACTIVE";
        case POLYX_EVENT_STATUS_PAUSED: return "PAUSED";
        case POLYX_EVENT_STATUS_ERROR: return "ERROR";
        case POLYX_EVENT_STATUS_CLOSED: return "CLOSED";
        default: return "UNKNOWN";
    }
}

static inline const char* polyx_error_str(PolyxError error) {
    switch (error) {
        case POLYX_OK: return "OK";
        case POLYX_ERROR_INVALID_PARAM: return "Invalid parameter";
        case POLYX_ERROR_NO_MEMORY: return "Out of memory";
        case POLYX_ERROR_NOT_SUPPORTED: return "Operation not supported";
        case POLYX_ERROR_WOULD_BLOCK: return "Operation would block";
        case POLYX_ERROR_IO: return "I/O error";
        case POLYX_ERROR_NETWORK: return "Network error";
        case POLYX_ERROR_TIMEOUT: return "Operation timed out";
        default: return "Unknown error";
    }
}

// Debug macros
#define POLYX_DEBUG(self, level, fmt, ...) \
    if ((self)->debug_callback && (self)->debug_level >= (level)) { \
        char _debug_msg[256]; \
        snprintf(_debug_msg, sizeof(_debug_msg), fmt, ##__VA_ARGS__); \
        (self)->debug_callback(level, __FILE__, __LINE__, __func__, _debug_msg); \
    }

#define POLYX_ERROR(self, fmt, ...) POLYX_DEBUG(self, POLYX_DEBUG_ERROR, fmt, ##__VA_ARGS__)
#define POLYX_WARN(self, fmt, ...)  POLYX_DEBUG(self, POLYX_DEBUG_WARN, fmt, ##__VA_ARGS__)
#define POLYX_INFO(self, fmt, ...)  POLYX_DEBUG(self, POLYX_DEBUG_INFO, fmt, ##__VA_ARGS__)
#define POLYX_DEBUG_MSG(self, fmt, ...) POLYX_DEBUG(self, POLYX_DEBUG_DEBUG, fmt, ##__VA_ARGS__)
#define POLYX_TRACE(self, fmt, ...) POLYX_DEBUG(self, POLYX_DEBUG_TRACE, fmt, ##__VA_ARGS__)

#endif // POLYX_ASYNC_H

