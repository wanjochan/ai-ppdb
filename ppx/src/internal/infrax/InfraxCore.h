#ifndef PPDB_INFRAX_CORE_H
#define PPDB_INFRAX_CORE_H

#include "cosmopolitan.h"
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
    
    // Network byte order conversion
    uint16_t (*host_to_net16)(struct InfraxCore *self, uint16_t host16);  // Host to network (16-bit)
    uint32_t (*host_to_net32)(struct InfraxCore *self, uint32_t host32);  // Host to network (32-bit)
    uint64_t (*host_to_net64)(struct InfraxCore *self, uint64_t host64);  // Host to network (64-bit)
    uint16_t (*net_to_host16)(struct InfraxCore *self, uint16_t net16);   // Network to host (16-bit)
    uint32_t (*net_to_host32)(struct InfraxCore *self, uint32_t net32);   // Network to host (32-bit)
    uint64_t (*net_to_host64)(struct InfraxCore *self, uint64_t net64);   // Network to host (64-bit)
};

extern InfraxCore g_infrax_core;  // global infrax core for tricks
InfraxCore* get_global_infrax_core(void);

#endif // PPDB_INFRAX_CORE_H
