#ifndef PPDB_INFRAX_CORE_H
#define PPDB_INFRAX_CORE_H

//design pattern: singleton

#include "cosmopolitan.h"

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
typedef int InfraxBool;
typedef int InfraxInt;

typedef clock_t InfraxClock;
typedef time_t InfraxTime;
// typedef InfraxU64 InfraxTime;//

#define INFRAX_TRUE  1
#define INFRAX_FALSE 0

// Forward declaration
typedef struct InfraxError InfraxError;
typedef struct InfraxCoreClassType InfraxCoreClassType;


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
//TODO
// Helper macro to create InfraxError with stack trace
static inline InfraxError make_error_with_stack(InfraxI32 code, const char* msg) {
    InfraxError err = {.code = code};
    if (msg) {
        strncpy(err.message, msg, sizeof(err.message) - 1);
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

//TODO 后面全部同意改用 make_error()
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

// Core structure definition
struct InfraxCore {
    // Instance pointer
    InfraxCore* self;

    // Core functions
    void* (*forward_call)(InfraxCore *self, void* (*target_func)(), ...);
    int (*printf)(InfraxCore *self, const char* format, ...);
    int (*snprintf)(InfraxCore *self, char* str, size_t size, const char* format, ...);

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
    void (*hint_yield)(struct InfraxCore *self);//hint only, not guaranteed to yield
    int (*pid)(struct InfraxCore *self);
    
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
    InfraxError (*file_exists)(struct InfraxCore *self, const char* path, bool* exists);

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
    // Time management
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
};

// "Class" for static methods
struct InfraxCoreClassType {
    InfraxCore* (*singleton)(void);  // Singleton getter
};
extern InfraxCoreClassType InfraxCoreClass;

// Assert macros
#define INFRAX_ASSERT(core, expr) \
    ((expr) ? (void)0 : (core)->assert_failed(core, __FILE__, __LINE__, __func__, #expr, NULL))

#define INFRAX_ASSERT_MSG(core, expr, msg) \
    ((expr) ? (void)0 : (core)->assert_failed(core, __FILE__, __LINE__, __func__, #expr, msg))

#endif // PPDB_INFRAX_CORE_H
