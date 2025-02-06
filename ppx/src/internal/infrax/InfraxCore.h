#ifndef PPDB_INFRAX_CORE_H
#define PPDB_INFRAX_CORE_H

#include "cosmopolitan.h"
// #include "internal/infrax/InfraxError.h"

//-----------------------------------------------------------------------------
// Basic Types
//-----------------------------------------------------------------------------

typedef int32_t InfraxI32;
typedef uint32_t InfraxU32;
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
// typedef int InfraxBool;

// #define INFRAX_TRUE  1
// #define INFRAX_FALSE 0

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

    // Time management
    InfraxTime (*time_now_ms)(InfraxCore *self);
    InfraxTime (*time_monotonic_ms)(struct InfraxCore *self);
    void (*sleep_ms)(struct InfraxCore *self, uint32_t milliseconds);
};

extern InfraxCore g_infrax_core;  // global infrax core for tricks
InfraxCore* get_global_infrax_core(void);

#endif // PPDB_INFRAX_CORE_H
