#ifndef INFRAX_ASYNC_H
#define INFRAX_ASYNC_H

/** DESIGN NOTES

//design pattern: factory
//main idea: mux of (setjmp/longjmp + poll)

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

#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <poll.h>

// Forward declarations
struct InfraxAsync;
struct InfraxPollInfo;
struct InfraxPollset;
struct InfraxAsyncContext;

// Type definitions
typedef void (*AsyncFunction)(struct InfraxAsync* self, void* arg);
typedef void (*PollCallback)(int fd, short events, void* arg);
typedef void (*TimerCallback)(void* arg);

// Async task states
typedef enum {
    INFRAX_ASYNC_PENDING,
    INFRAX_ASYNC_FULFILLED,
    INFRAX_ASYNC_REJECTED
} InfraxAsyncState;

// Poll events (compatible with <poll.h>)
#define INFRAX_POLLIN      0x001  /* There is data to read */
#define INFRAX_POLLOUT     0x004  /* Writing now will not block */
#define INFRAX_POLLERR     0x008  /* Error condition */
#define INFRAX_POLLHUP     0x010  /* Hung up */
#define INFRAX_POLLNVAL    0x020  /* Invalid request: fd not open */

// Poll info structure
struct InfraxPollInfo {
    int fd;
    short events;
    PollCallback callback;
    void* arg;
    struct InfraxPollInfo* next;
};

// Pollset structure
struct InfraxPollset {
    struct pollfd* fds;
    struct InfraxPollInfo** infos;
    size_t size;
    size_t capacity;
};

// Async context structure
struct InfraxAsyncContext {
    jmp_buf env;
    void* stack;
    size_t stack_size;
    int yield_count;
    struct InfraxPollset pollset;
};

// Async task structure
typedef struct InfraxAsync {
    AsyncFunction fn;
    void* arg;
    InfraxAsyncState state;
    void* ctx;
    void* user_data;
    size_t user_data_size;
    struct InfraxAsync* next;
    int error;
} InfraxAsync;

// Async class interface
typedef struct {
    // Task management
    InfraxAsync* (*new)(AsyncFunction fn, void* arg);
    void (*free)(InfraxAsync* self);
    InfraxAsync* (*start)(InfraxAsync* self);
    void (*cancel)(InfraxAsync* self);
    void (*yield)(InfraxAsync* self);
    
    // Result handling
    void (*set_result)(InfraxAsync* self, void* data, size_t size);
    void* (*get_result)(InfraxAsync* self, size_t* size);
    
    // Poll operations
    int (*pollset_add_fd)(InfraxAsync* self, int fd, short events, PollCallback cb, void* arg);
    int (*pollset_remove_fd)(InfraxAsync* self, int fd);
    int (*pollset_poll)(InfraxAsync* self, int timeout_ms);  // timeout_ms: -1 for infinite, 0 for immediate return

    // State checking
    bool (*is_done)(InfraxAsync* self);
} InfraxAsyncClass_t;

// Global class instance
extern const InfraxAsyncClass_t InfraxAsyncClass;

#endif // INFRAX_ASYNC_H
