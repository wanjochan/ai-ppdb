#ifndef PPDB_INFRAX_CORE_H
#define PPDB_INFRAX_CORE_H

//design pattern: singleton
//-----------------------------------------------------------------------------
// Basic Types
//-----------------------------------------------------------------------------
typedef int8_t InfraxI8;
typedef uint8_t InfraxU8;
typedef int16_t InfraxI16;
typedef uint16_t InfraxU16;
typedef int32_t InfraxI32;
typedef uint32_t InfraxU32;
typedef int64_t InfraxI64;
typedef uint64_t InfraxU64;
typedef size_t InfraxSize;
typedef ssize_t InfraxSSize;
typedef uint8_t InfraxBool;
typedef int InfraxInt;//尽量不用

typedef InfraxU64 InfraxClock;
typedef InfraxU64 InfraxTime;

// Time value structure
typedef struct InfraxTimeVal {
    InfraxI64 tv_sec;    // seconds
    InfraxI64 tv_usec;   // microseconds
} InfraxTimeVal;

#define INFRAX_VOID void
#define INFRAX_TRUE ((InfraxBool)1)
#define INFRAX_FALSE ((InfraxBool)0)

//-----------------------------------------------------------------------------
// Network Types
//-----------------------------------------------------------------------------
// 网络地址族
#define INFRAX_AF_INET 2  // IPv4

// 网络协议族
#define INFRAX_PF_INET INFRAX_AF_INET

// Socket 类型
#define INFRAX_SOCK_STREAM 1  // TCP
#define INFRAX_SOCK_DGRAM  2  // UDP

// 网络协议
#define INFRAX_IPPROTO_TCP 6
#define INFRAX_IPPROTO_UDP 17

// Socket 选项级别
#define INFRAX_SOL_SOCKET 0xffff

// Socket 选项
#define INFRAX_SO_REUSEADDR 0x0004
#define INFRAX_SO_KEEPALIVE 0x0008
#define INFRAX_SO_RCVTIMEO 0x1006
#define INFRAX_SO_SNDTIMEO 0x1005
#define INFRAX_SO_RCVBUF 0x1002
#define INFRAX_SO_SNDBUF 0x1001
#define INFRAX_SO_ERROR 0x1007

// Socket shutdown 方式
#define INFRAX_SHUT_RD   0
#define INFRAX_SHUT_WR   1
#define INFRAX_SHUT_RDWR 2

// 错误码
#define INFRAX_EAGAIN      11
#define INFRAX_EWOULDBLOCK INFRAX_EAGAIN
#define INFRAX_EINPROGRESS 115
#define INFRAX_ENOTCONN    107
#define INFRAX_ECONNREFUSED 111
#define INFRAX_ETIMEDOUT   110
#define INFRAX_EINVAL      22
#define INFRAX_EBADF       9
#define INFRAX_ENOTSOCK    88
#define INFRAX_EOPNOTSUPP  95
#define INFRAX_EADDRINUSE  98
#define INFRAX_EADDRNOTAVAIL 99
#define INFRAX_EAFNOSUPPORT 97
#define INFRAX_EALREADY    114
#define INFRAX_EISCONN     106
#define INFRAX_EMSGSIZE    90
#define INFRAX_ENOBUFS     105
#define INFRAX_ENOMEM      12
#define INFRAX_EPIPE       32

// 网络地址结构 （TODO 要加 ipv6 地址？）
typedef struct InfraxInAddr {
    InfraxU32 s_addr;  // IPv4 地址，网络字节序
} InfraxInAddr;

typedef struct InfraxSockAddrIn {
    InfraxU16 sin_family;     // 地址族 (AF_INET)
    InfraxU16 sin_port;       // 端口号，网络字节序
    InfraxInAddr sin_addr;    // IPv4 地址
    InfraxU8 sin_zero[8];     // 填充字节
} InfraxSockAddrIn;

typedef struct InfraxSockAddr {
    InfraxU16 sa_family;      // 地址族
    InfraxU8 sa_data[14];     // 协议地址
} InfraxSockAddr;

// Forward declarations
typedef struct InfraxCore InfraxCore;
typedef struct InfraxCoreClassType InfraxCoreClassType;
typedef struct InfraxError InfraxError;

// Error structure definition
struct InfraxError {
    InfraxError* self;//point to self
    // Error state
    InfraxI32 code;
    char message[128];
    #ifdef INFRAX_ENABLE_STACKTRACE
    void* stack_frames[32];
    int stack_depth;
    char stack_trace[1024];
    #endif
};

typedef InfraxU32 InfraxFlags;
typedef InfraxU64 InfraxHandle;

// Buffer Types
typedef struct InfraxBuffer {
    InfraxU8* data;
    InfraxSize size;
    InfraxSize capacity;
} InfraxBuffer;

typedef struct InfraxRingBuffer {
    InfraxU8* buffer;
    InfraxSize size;
    InfraxSize read_pos;
    InfraxSize write_pos;
    InfraxBool full;
} InfraxRingBuffer;

// File Operation Flags
#define INFRAX_FILE_CREATE (1 << 0)
#define INFRAX_FILE_RDONLY (1 << 1)
#define INFRAX_FILE_WRONLY (1 << 2)
#define INFRAX_FILE_RDWR   (1 << 3)
#define INFRAX_FILE_APPEND (1 << 4)
#define INFRAX_FILE_TRUNC  (1 << 5)

#define INFRAX_SEEK_SET 0
#define INFRAX_SEEK_CUR 1
#define INFRAX_SEEK_END 2

// Error codes
#define INFRAX_ERROR_OK 0
#define INFRAX_ERROR_UNKNOWN -1
#define INFRAX_ERROR_NO_MEMORY -2
#define INFRAX_ERROR_INVALID_PARAM -3
#define INFRAX_ERROR_FILE_NOT_FOUND -4
#define INFRAX_ERROR_FILE_ACCESS -5
#define INFRAX_ERROR_FILE_READ -6
#define INFRAX_ERROR_TIMEOUT -7
#define INFRAX_ERROR_SYSTEM 5  // 系统级错误(如pthread, pipe等)
#define INFRAX_ERROR_FILE_EXISTS -8  // 文件已存在
#define INFRAX_ERROR_WRITE_FAILED -9  // 写入失败

//-----------------------------------------------------------------------------
// Thread Types
//-----------------------------------------------------------------------------

typedef void* InfraxMutex;
typedef void* InfraxMutexAttr;
typedef void* InfraxCond;
typedef void* InfraxCondAttr;
typedef void* InfraxThreadAttr;
typedef void* (*InfraxThreadFunc)(void*);

// Assert macros and functions
#define INFRAX_ASSERT_FAILED_CODE -1000

// Assert handler type definition
typedef void (*InfraxAssertHandler)(const char* file, int line, const char* func, const char* expr, const char* msg);

// Time spec structure
typedef struct {
    int64_t tv_sec;
    int64_t tv_nsec;
} InfraxTimeSpec;

#define INFRAX_CLOCK_REALTIME  0
#define INFRAX_CLOCK_MONOTONIC 1

// Signal definitions
#define INFRAX_SIGALRM 14  // SIGALRM 的标准值

typedef void (*InfraxSignalHandler)(int);

// File descriptor set structure
typedef struct InfraxFdSet {
    unsigned long fds_bits[1024 / (8 * sizeof(unsigned long))];
} InfraxFdSet;

// File descriptor operations flags
#define INFRAX_F_GETFL  3   // Get file status flags
#define INFRAX_F_SETFL  4   // Set file status flags
#define INFRAX_F_GETFD  1   // Get file descriptor flags
#define INFRAX_F_SETFD  2   // Set file descriptor flags

// File status flags
#define INFRAX_O_NONBLOCK  0x00004000  // Non-blocking mode

// File descriptor macros
#define INFRAX_FD_SETSIZE 1024

// Error codes
#define INFRAX_ERROR_IO_INVALID_FD -200
#define INFRAX_ERROR_IO_FCNTL_FAILED -201
#define INFRAX_ERROR_IO_SELECT_FAILED -202
#define INFRAX_ERROR_IO_TIMEOUT -203
#define INFRAX_ERROR_IO_INTERRUPTED -204

// Poll events
#define INFRAX_POLLIN  0x001  // 有数据可读
#define INFRAX_POLLOUT 0x004  // 可以写数据
#define INFRAX_POLLERR 0x008  // 发生错误
#define INFRAX_POLLHUP 0x010  // 挂起
#define INFRAX_POLLNVAL 0x020 // 文件描述符未打开

// Poll structure
typedef struct InfraxPollFd {
    int fd;             // 文件描述符
    short events;       // 请求的事件
    short revents;      // 返回的事件
} InfraxPollFd;

// Core structure definition
struct InfraxCore {
    // Instance pointer
    InfraxCore* self;
    InfraxCoreClassType* klass;//InfraxCoreClass

    // Core functions
    void* (*forward_call)(InfraxCore *self, void* (*target_func)(), ...);
    int (*printf)(InfraxCore *self, const char* format, ...);
    int (*snprintf)(InfraxCore *self, char* str, size_t size, const char* format, ...);
    int (*fprintf)(InfraxCore *self, int* stream, const char* format, ...);
    
    // core (base) memory operations (for high level need to use InfraxMemory)
    void* (*malloc)(InfraxCore *self, size_t size);
    void* (*calloc)(InfraxCore *self, size_t nmemb, size_t size);
    void* (*realloc)(InfraxCore *self, void* ptr, size_t size);
    void (*free)(InfraxCore *self, void* ptr);

    // String operations
    size_t (*strlen)(InfraxCore *self, const char* s);
    char* (*strcpy)(InfraxCore *self, char* dest, const char* src);
    char* (*strncpy)(InfraxCore *self, char* dest, const char* src, size_t n);
    char* (*strcat)(InfraxCore *self, char* dest, const char* src);
    char* (*strncat)(InfraxCore *self, char* dest, const char* src, size_t n);
    int (*strcmp)(InfraxCore *self, const char* s1, const char* s2);
    int (*strncmp)(InfraxCore *self, const char* s1, const char* s2, size_t n);
    char* (*strchr)(InfraxCore *self, const char* s, int c);
    char* (*strrchr)(InfraxCore *self, const char* s, int c);
    char* (*strstr)(InfraxCore *self, const char* haystack, const char* needle);
    char* (*strdup)(InfraxCore *self, const char* s);
    char* (*strndup)(InfraxCore *self, const char* s, size_t n);

    //Misc operations
    int (*memcmp)(struct InfraxCore *self, const void* s1, const void* s2, size_t n);
    void* (*memset)(struct InfraxCore *self, void* s, int c, size_t n);
    void* (*memcpy)(struct InfraxCore *self, void* dest, const void* src, size_t n);
    void* (*memmove)(struct InfraxCore *self, void* dest, const void* src, size_t n);
    int (*isspace)(struct InfraxCore *self, int c);
    int (*isdigit)(struct InfraxCore *self, int c);
    void (*hint_yield)(struct InfraxCore *self);//hint only, not guaranteed to yield
    int (*pid)(struct InfraxCore *self);
    
    // String to number conversion
    int (*atoi)(struct InfraxCore *self, const char* str);
    long (*atol)(struct InfraxCore *self, const char* str);
    long long (*atoll)(struct InfraxCore *self, const char* str);
    
    // Random number operations
    InfraxU32 (*random)(struct InfraxCore *self);          // Generate random number
    void (*random_seed)(struct InfraxCore *self, uint32_t seed);  // Set random seed
    
    // Network byte order conversion
    InfraxU16 (*host_to_net16)(struct InfraxCore *self, InfraxU16 host16);  // Host to network (16-bit)
    InfraxU32 (*host_to_net32)(struct InfraxCore *self, InfraxU32 host32);  // Host to network (32-bit)
    InfraxU64 (*host_to_net64)(struct InfraxCore *self, InfraxU64 host64);  // Host to network (64-bit)
    InfraxU16 (*net_to_host16)(struct InfraxCore *self, InfraxU16 net16);   // Network to host (16-bit)
    InfraxU32 (*net_to_host32)(struct InfraxCore *self, InfraxU32 net32);   // Network to host (32-bit)
    InfraxU64 (*net_to_host64)(struct InfraxCore *self, InfraxU64 net64);   // Network to host (64-bit)

    // Buffer operations
    InfraxError (*buffer_init)(struct InfraxCore *self, InfraxBuffer* buf, size_t initial_capacity);
    void (*buffer_destroy)(struct InfraxCore *self, InfraxBuffer* buf);
    InfraxError (*buffer_reserve)(struct InfraxCore *self, InfraxBuffer* buf, size_t capacity);
    InfraxError (*buffer_write)(struct InfraxCore *self, InfraxBuffer* buf, const void* data, size_t size);
    InfraxError (*buffer_read)(struct InfraxCore *self, InfraxBuffer* buf, void* data, size_t size);
    size_t (*buffer_readable)(struct InfraxCore *self, const InfraxBuffer* buf);
    size_t (*buffer_writable)(struct InfraxCore *self, const InfraxBuffer* buf);
    void (*buffer_reset)(struct InfraxCore *self, InfraxBuffer* buf);

    // Ring buffer operations
    InfraxError (*ring_buffer_init)(struct InfraxCore *self, InfraxRingBuffer* rb, size_t size);
    void (*ring_buffer_destroy)(struct InfraxCore *self, InfraxRingBuffer* rb);
    InfraxError (*ring_buffer_write)(struct InfraxCore *self, InfraxRingBuffer* rb, const void* data, size_t size);
    InfraxError (*ring_buffer_read)(struct InfraxCore *self, InfraxRingBuffer* rb, void* data, size_t size);
    InfraxSize (*ring_buffer_readable)(struct InfraxCore *self, const InfraxRingBuffer* rb);
    InfraxSize (*ring_buffer_writable)(struct InfraxCore *self, const InfraxRingBuffer* rb);
    void (*ring_buffer_reset)(struct InfraxCore *self, InfraxRingBuffer* rb);

    // File operations (synchronous)
    InfraxError (*file_open)(struct InfraxCore *self, const char* path, InfraxFlags flags, int mode, InfraxHandle* handle);
    InfraxError (*file_close)(struct InfraxCore *self, InfraxHandle handle);
    InfraxError (*file_read)(struct InfraxCore *self, InfraxHandle handle, void* buffer, size_t size, size_t* bytes_read);
    InfraxError (*file_write)(struct InfraxCore *self, InfraxHandle handle, const void* buffer, size_t size, size_t* bytes_written);
    InfraxError (*file_seek)(struct InfraxCore *self, InfraxHandle handle, int64_t offset, int whence);
    InfraxError (*file_size)(struct InfraxCore *self, InfraxHandle handle, size_t* size);
    InfraxError (*file_remove)(struct InfraxCore *self, const char* path);
    InfraxError (*file_rename)(struct InfraxCore *self, const char* old_path, const char* new_path);
    InfraxError (*file_exists)(struct InfraxCore *self, const char* path, InfraxBool* exists);

    // Assert functions
    void (*assert_failed)(struct InfraxCore *self, const char* file, int line, const char* func, const char* expr, const char* msg);
    void (*set_assert_handler)(struct InfraxCore *self, InfraxAssertHandler handler);

    // File descriptor operations (for async now)
    InfraxSSize (*read_fd)(InfraxCore *self, int fd, void* buf, InfraxSize count);
    InfraxSSize (*write_fd)(InfraxCore *self, int fd, const void* buf, InfraxSize count);
    int (*create_pipe)(InfraxCore *self, int pipefd[2]);
    int (*set_nonblocking)(InfraxCore *self, int fd);
    int (*close_fd)(InfraxCore *self, int fd);

    // Time operations
    InfraxTime (*localtime)(InfraxCore *self, InfraxTime* tloc);
    InfraxTime (*time_now_ms)(InfraxCore *self);
    InfraxTime (*time_monotonic_ms)(struct InfraxCore *self);
    InfraxClock (*clock)(InfraxCore *self);
    int (*clock_gettime)(InfraxCore *self, int clk_id, InfraxTimeSpec* tp);
    InfraxTime (*time)(InfraxCore *self, InfraxTime* tloc);
    int (*clocks_per_sec)(InfraxCore *self);
    void (*sleep)(InfraxCore *self, unsigned int seconds);
    void (*sleep_ms)(struct InfraxCore *self, uint32_t milliseconds);
    void (*sleep_us)(InfraxCore *self, unsigned int microseconds);
    InfraxSize (*get_memory_usage)(InfraxCore *self);

    // Signal operations
    InfraxSignalHandler (*signal)(InfraxCore *self, int signum, InfraxSignalHandler handler);
    unsigned int (*alarm)(InfraxCore *self, unsigned int seconds);

    // Network operations
    intptr_t (*socket_create)(struct InfraxCore* self, int domain, int type, int protocol);
    int (*socket_bind)(struct InfraxCore* self, intptr_t handle, const void* addr, size_t addr_len);
    int (*socket_listen)(struct InfraxCore* self, intptr_t handle, int backlog);
    intptr_t (*socket_accept)(struct InfraxCore* self, intptr_t handle, void* addr, size_t* addr_len);
    int (*socket_connect)(struct InfraxCore* self, intptr_t handle, const void* addr, size_t addr_len);
    ssize_t (*socket_send)(struct InfraxCore* self, intptr_t handle, const void* buf, size_t len, int flags);
    ssize_t (*socket_recv)(struct InfraxCore* self, intptr_t handle, void* buf, size_t len, int flags);
    int (*socket_close)(struct InfraxCore* self, intptr_t handle);
    int (*socket_shutdown)(struct InfraxCore* self, intptr_t handle, int how);
    int (*socket_set_option)(struct InfraxCore* self, intptr_t handle, int level, int option, const void* value, size_t len);
    int (*socket_get_option)(struct InfraxCore* self, intptr_t handle, int level, int option, void* value, size_t* len);
    int (*socket_get_error)(struct InfraxCore* self, intptr_t handle);
    int (*socket_get_name)(struct InfraxCore* self, intptr_t handle, void* addr, size_t* addr_len);
    int (*socket_get_peer)(struct InfraxCore* self, intptr_t handle, void* addr, size_t* addr_len);

    // Network address operations
    int (*ip_to_binary)(struct InfraxCore* self, const char* ip, void* addr, size_t size);
    int (*binary_to_ip)(struct InfraxCore* self, const void* addr, char* ip, size_t size);

    // Error handling
    int (*get_last_error)(struct InfraxCore* self);
    const char* (*get_error_string)(struct InfraxCore* self, int error_code);

    // File descriptor set operations
    void (*fd_zero)(struct InfraxCore* self, InfraxFdSet* set);
    void (*fd_set)(struct InfraxCore* self, int fd, InfraxFdSet* set);
    void (*fd_clr)(struct InfraxCore* self, int fd, InfraxFdSet* set);
    int (*fd_isset)(struct InfraxCore* self, int fd, InfraxFdSet* set);

    // File descriptor control operations
    int (*fcntl)(struct InfraxCore* self, int fd, int cmd, int arg);

    // IO multiplexing
    int (*select)(struct InfraxCore* self, int nfds, InfraxFdSet* readfds, 
                  InfraxFdSet* writefds, InfraxFdSet* exceptfds, InfraxTimeVal* timeout);

    // High-level IO operations
    InfraxError (*wait_for_read)(struct InfraxCore* self, int fd, int timeout_ms);
    InfraxError (*wait_for_write)(struct InfraxCore* self, int fd, int timeout_ms);
    InfraxError (*wait_for_except)(struct InfraxCore* self, int fd, int timeout_ms);

    // Poll operations
    int (*poll)(struct InfraxCore* self, InfraxPollFd* fds, size_t nfds, int timeout_ms);
    
    // Timer operations
    InfraxU32 (*timer_create)(struct InfraxCore* self, InfraxU32 interval_ms, InfraxBool is_interval, void (*handler)(void*), void* arg);
    InfraxError (*timer_delete)(struct InfraxCore* self, InfraxU32 timer_id);
    InfraxError (*timer_reset)(struct InfraxCore* self, InfraxU32 timer_id, InfraxU32 interval_ms);
    InfraxError (*timer_pause)(struct InfraxCore* self, InfraxU32 timer_id);
    InfraxError (*timer_resume)(struct InfraxCore* self, InfraxU32 timer_id);
};

// "Class" for static methods
struct InfraxCoreClassType {
    InfraxCore* (*singleton)(void);  // Singleton getter
};

extern InfraxCore gInfraxCore;//faster access to the singleton
extern InfraxCoreClassType InfraxCoreClass;

// Helper macro to create InfraxError with stack trace
static inline InfraxError make_error_with_stack(InfraxI32 code, const char* msg) {
    InfraxError err = {.code = code};
    if (msg) {
        InfraxCore* core = &gInfraxCore;//InfraxCoreClass.singleton();
        core->strncpy(core, err.message, msg, sizeof(err.message) - 1);
        err.message[sizeof(err.message) - 1] = '\0';
    }
    
    #ifdef INFRAX_ENABLE_STACKTRACE
    err.stack_depth = backtrace(err.stack_frames, 32);
    char** symbols = backtrace_symbols(err.stack_frames, err.stack_depth);
    if (symbols) {
        int pos = 0;
        for (int i = 0; i < err.stack_depth && pos < sizeof(err.stack_trace) - 1; i++) {
            pos += snprintf(err.stack_trace + pos, sizeof(err.stack_trace) - pos, 
                          "%s\n", symbols[i]);
        }
        free(symbols);
    }
    #endif
    
    return err;
}

// Helper macro to create InfraxError
static inline InfraxError make_error(InfraxI32 code, const char* msg) {
    InfraxError err = {.code = code};
    if (msg) {
        InfraxCore* core = InfraxCoreClass.singleton();
        core->strncpy(core, err.message, msg, sizeof(err.message) - 1);
        err.message[sizeof(err.message) - 1] = '\0';
    } else {
        err.message[0] = '\0';
    }
    return err;
}

//TODO 后面全部同意改用 make_error()
#define INFRAX_ERROR_OK_STRUCT (InfraxError){.code = INFRAX_ERROR_OK, .message = ""}

// Helper macro to compare InfraxError
#define INFRAX_ERROR_IS_OK(err) ((err).code == INFRAX_ERROR_OK)
#define INFRAX_ERROR_IS_ERR(err) ((err).code != INFRAX_ERROR_OK)

// Assert macros
#define INFRAX_ASSERT(core, expr) \
    ((expr) ? (void)0 : (core)->assert_failed(core, __FILE__, __LINE__, __func__, #expr, NULL))

#define INFRAX_ASSERT_MSG(core, expr, msg) \
    ((expr) ? (void)0 : (core)->assert_failed(core, __FILE__, __LINE__, __func__, #expr, msg))

#endif // PPDB_INFRAX_CORE_H
