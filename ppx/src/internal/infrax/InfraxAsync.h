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

// #include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Forward declarations
typedef struct InfraxAsync InfraxAsync;
typedef void (*AsyncFunction)(InfraxAsync* self, void* arg);
typedef void (*PollCallback)(int fd, short revents, void* arg);

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

// Internal pollset structure
typedef struct InfraxPollInfo {
    int fd;                 // File descriptor
    short events;          // Events to watch for
    PollCallback callback; // Callback function
    void* arg;            // Callback argument
    struct InfraxPollInfo* next;  // Next in list
} InfraxPollInfo;

// Async task structure
struct InfraxAsync {
    AsyncFunction fn;          // Task function
    void* arg;                // Function argument
    void* ctx;                // Execution context
    void* user_data;          // User data storage
    size_t user_data_size;    // Size of user data
    int error;                // Error code
    InfraxAsyncState state;   // Task state
    InfraxAsync* next;        // Next task in queue
};

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
