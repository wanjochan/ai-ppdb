#ifndef PPDB_INFRAX_CORE_H
#define PPDB_INFRAX_CORE_H

#include "cosmopolitan.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
// #include "internal/infrax/InfraxError.h"

//-----------------------------------------------------------------------------
// Basic Types
//-----------------------------------------------------------------------------

typedef int32_t InfraxI32;
typedef uint32_t InfraxU32;
typedef int InfraxBool;

#define INFRAX_TRUE  1
#define INFRAX_FALSE 0

// Forward declaration
typedef struct InfraxError InfraxError;

// Error structure definition
struct InfraxError {
    // Error state
    InfraxI32 code;
    char message[128];
    // TODO: Add stack trace support when solution is available
    // void* stack_frames[32];
    // int stack_depth;
    // (char stack_trace[1024])dump_stack()
};

typedef uint64_t InfraxTime;//
typedef uint32_t InfraxFlags;
typedef uint64_t InfraxHandle;

// Buffer Types
typedef struct InfraxBuffer {
    uint8_t* data;
    size_t size;
    size_t capacity;
} InfraxBuffer;

typedef struct InfraxRingBuffer {
    uint8_t* buffer;
    size_t size;
    size_t read_pos;
    size_t write_pos;
    bool full;
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

#define INFRAX_OK 0
#define INFRAX_ERROR_OK 0
#define INFRAX_ERROR_INVALID_PARAM -1
#define INFRAX_ERROR_NO_MEMORY -2

// Helper macro to create InfraxError
static inline InfraxError make_error(InfraxI32 code, const char* msg) {
    InfraxError err = {.code = code};
    if (msg) {
        strncpy(err.message, msg, sizeof(err.message) - 1);
        err.message[sizeof(err.message) - 1] = '\0';
    } else {
        err.message[0] = '\0';
    }
    return err;
}

#define INFRAX_ERROR_OK_STRUCT (InfraxError){.code = INFRAX_ERROR_OK, .message = ""}

// Helper macro to compare InfraxError
#define INFRAX_ERROR_IS_OK(err) ((err).code == INFRAX_ERROR_OK)
#define INFRAX_ERROR_IS_ERR(err) ((err).code != INFRAX_ERROR_OK)

//-----------------------------------------------------------------------------
// Thread Types
//-----------------------------------------------------------------------------

typedef void* InfraxMutex;
typedef void* InfraxMutexAttr;
typedef void* InfraxCond;
typedef void* InfraxCondAttr;
typedef void* InfraxThreadAttr;
typedef void* (*InfraxThreadFunc)(void*);

// Forward declaration
typedef struct InfraxCore InfraxCore;

// Core structure definition
struct InfraxCore {
    // core 特别，不需要构建的，完全全局，用来放全局静态函数
    // Printf forwarding
    int (*printf)(InfraxCore *self, const char* format, ...);
    
    // Parameter forwarding function
    void* (*forward_call)(InfraxCore *self, void* (*target_func)(), ...);

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

    // Time management
    InfraxTime (*time_now_ms)(InfraxCore *self);
    InfraxTime (*time_monotonic_ms)(struct InfraxCore *self);
    void (*sleep_ms)(struct InfraxCore *self, uint32_t milliseconds);
    /*
    在 InfraxCore 中实现这个功能，说明这个框架考虑到了多线程场景下的性能优化需求。它可以被用在：
自旋锁的实现中
等待队列的实现中
需要让出 CPU 的协作式多任务处理中
不过需要注意的是，yield 只是一个提示（hint）给操作系统，具体是否真的切换到其他线程，还是由操作系统的调度器决定的。
    */
    void (*yield)(struct InfraxCore *self);
    //getpid
    int (*pid)(struct InfraxCore *self);
    
    // Random number operations
    InfraxU32 (*random)(struct InfraxCore *self);          // Generate random number
    void (*random_seed)(struct InfraxCore *self, uint32_t seed);  // Set random seed
    
    // Network byte order conversion
    uint16_t (*host_to_net16)(struct InfraxCore *self, uint16_t host16);  // Host to network (16-bit)
    uint32_t (*host_to_net32)(struct InfraxCore *self, uint32_t host32);  // Host to network (32-bit)
    uint64_t (*host_to_net64)(struct InfraxCore *self, uint64_t host64);  // Host to network (64-bit)
    uint16_t (*net_to_host16)(struct InfraxCore *self, uint16_t net16);   // Network to host (16-bit)
    uint32_t (*net_to_host32)(struct InfraxCore *self, uint32_t net32);   // Network to host (32-bit)
    uint64_t (*net_to_host64)(struct InfraxCore *self, uint64_t net64);   // Network to host (64-bit)

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
    size_t (*ring_buffer_readable)(struct InfraxCore *self, const InfraxRingBuffer* rb);
    size_t (*ring_buffer_writable)(struct InfraxCore *self, const InfraxRingBuffer* rb);
    void (*ring_buffer_reset)(struct InfraxCore *self, InfraxRingBuffer* rb);

    // File operations
    InfraxError (*file_open)(struct InfraxCore *self, const char* path, InfraxFlags flags, int mode, InfraxHandle* handle);
    InfraxError (*file_close)(struct InfraxCore *self, InfraxHandle handle);
    InfraxError (*file_read)(struct InfraxCore *self, InfraxHandle handle, void* buffer, size_t size, size_t* bytes_read);
    InfraxError (*file_write)(struct InfraxCore *self, InfraxHandle handle, const void* buffer, size_t size, size_t* bytes_written);
    InfraxError (*file_seek)(struct InfraxCore *self, InfraxHandle handle, int64_t offset, int whence);
    InfraxError (*file_size)(struct InfraxCore *self, InfraxHandle handle, size_t* size);
    InfraxError (*file_remove)(struct InfraxCore *self, const char* path);
    InfraxError (*file_rename)(struct InfraxCore *self, const char* old_path, const char* new_path);
    InfraxError (*file_exists)(struct InfraxCore *self, const char* path, bool* exists);
};

extern InfraxCore g_infrax_core;  // global infrax core for tricks
InfraxCore* get_global_infrax_core(void);

// TODO:重构，类似 InfraxAsync， static InfraxCoreClass 放这些静态方法就可以了
// infrax_core_new( )
// infrax_core_new
// InfraxAsync* infrax_async_new(AsyncFn fn, void* arg);
// void infrax_async_free(InfraxAsync* self);

// // Factory for InfraxAsync instances
// static struct {
//     InfraxAsync* (*new)(AsyncFn fn, void* arg);
//     void (*free)(InfraxAsync* self);
// } InfraxAsyncClass = {
//     .new = infrax_async_new,
//     .free = infrax_async_free
// };

InfraxCore* singleton();

#endif // PPDB_INFRAX_CORE_H
