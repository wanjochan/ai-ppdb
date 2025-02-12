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

// Event types
typedef enum {
    POLYX_EVENT_NONE = 0,
    POLYX_EVENT_IO,
    POLYX_EVENT_TIMER,
    POLYX_EVENT_SIGNAL
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

// Event structure
struct PolyxEvent {
    PolyxEventType type;
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
    InfraxAsync* infrax;
    PolyxEvent** events;
    size_t event_count;
    size_t event_capacity;
    
    // 对象方法 - 事件管理
    PolyxEvent* (*create_event)(struct PolyxAsync* self, PolyxEventConfig* config);
    PolyxEvent* (*create_timer)(struct PolyxAsync* self, PolyxTimerConfig* config);
    void (*destroy_event)(struct PolyxAsync* self, PolyxEvent* event);
      // 对象方法 - 事件触发
    void (*trigger_event)(struct PolyxAsync* self, PolyxEvent* event, void* data, size_t size);
     
    // 对象方法 - 定时器控制
    void (*start_timer)(struct PolyxAsync* self, PolyxEvent* timer);
    void (*stop_timer)(struct PolyxAsync* self, PolyxEvent* timer);
    
    //TODO 网络相关；
 
    // 对象方法 - 轮询
    int (*poll)(struct PolyxAsync* self, int timeout_ms);
};

// 类接口
typedef struct {
    PolyxAsync* (*new)(void);
    void (*free)(PolyxAsync* self);

    //有没有线程相关的处理需要搬过来？可能后面有一些工具方法，针对 poll 的类型做的便捷函数！
} PolyxAsyncClassType;

// 全局类实例
extern const PolyxAsyncClassType PolyxAsyncClass;

#endif // POLYX_ASYNC_H
