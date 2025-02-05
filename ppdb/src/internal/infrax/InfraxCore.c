// #include <stdarg.h>
// #include <stdio.h>
// #include <stdlib.h>
#include "cosmopolitan.h"
#include "internal/infrax/InfraxCore.h"

// Forward declarations of internal functions
// static void infrax_core_init(InfraxCore* self);
// static void infrax_core_print(InfraxCore* self);

// Helper function to create a new error value
static InfraxError infrax_core_new_error(InfraxI32 code, const char* message) {
    InfraxError error = {.code = code};
    if (message) {
        strncpy(error.message, message, sizeof(error.message) - 1);
    }
    error.message[sizeof(error.message) - 1] = '\0';  // Ensure null termination
    return error;
}

// Printf forwarding implementation
int infrax_core_printf(InfraxCore *self, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);
    return result;
}

// Parameter forwarding function implementation
void* infrax_core_forward_call(InfraxCore *self,void* (*target_func)(va_list), ...) {
    va_list args;
    va_start(args, target_func);
    void* result = target_func(args);
    va_end(args);
    return result;
}

// static void infrax_core_init(InfraxCore *self) {
//     if (!self) return;
    
//     // Initialize methods
//     self->new = infrax_core_new;
//     self->free = infrax_core_free;
//     // self->print = infrax_core_print;
//     self->forward_call = infrax_core_forward_call;
//     self->printf = infrax_core_printf;
// }

// // Private implementation
// struct InfraxCoreImpl {
//     InfraxCore interface;  // must be first
//     // private members if needed
// };

// Function implementations
static InfraxTime infrax_core_time_now_ms(InfraxCore *self) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static InfraxTime infrax_core_time_monotonic_ms(InfraxCore *self) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void infrax_core_sleep_ms(InfraxCore *self, uint32_t milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

// static InfraxError infrax_core_thread_create(InfraxCore *self, InfraxThread* thread, InfraxThreadFunc func, void* arg) {
//     pthread_t* pthread = malloc(sizeof(pthread_t));
//     if (!pthread) {
//         return INFRAX_ERROR_OUT_OF_MEMORY;
//     }
    
//     if (pthread_create(pthread, NULL, func, arg) != 0) {
//         free(pthread);
//         return INFRAX_ERROR_THREAD_CREATE;
//     }
    
//     *thread = pthread;
//     return INFRAX_OK;
// }

// static InfraxError infrax_core_thread_join(InfraxCore *self, InfraxThread thread) {
//     pthread_t* pthread = thread;
//     if (pthread_join(*pthread, NULL) != 0) {
//         return INFRAX_ERROR_THREAD_JOIN;
//     }
//     free(pthread);
//     return INFRAX_OK;
// }

// static InfraxError infrax_core_mutex_create(InfraxCore *self, InfraxMutex* mutex) {
//     pthread_mutex_t* pmutex = malloc(sizeof(pthread_mutex_t));
//     if (!pmutex) {
//         return INFRAX_ERROR_OUT_OF_MEMORY;
//     }
    
//     if (pthread_mutex_init(pmutex, NULL) != 0) {
//         free(pmutex);
//         return INFRAX_ERROR_MUTEX_CREATE;
//     }
    
//     *mutex = pmutex;
//     return INFRAX_OK;
// }

// static void infrax_core_mutex_destroy(InfraxCore *self, InfraxMutex mutex) {
//     pthread_mutex_t* pmutex = mutex;
//     pthread_mutex_destroy(pmutex);
//     free(pmutex);
// }

// static InfraxError infrax_core_mutex_lock(InfraxCore *self, InfraxMutex mutex) {
//     pthread_mutex_t* pmutex = mutex;
//     if (pthread_mutex_lock(pmutex) != 0) {
//         return INFRAX_ERROR_MUTEX_LOCK;
//     }
//     return INFRAX_OK;
// }

// static InfraxError infrax_core_mutex_unlock(InfraxCore *self, InfraxMutex mutex) {
//     pthread_mutex_t* pmutex = mutex;
//     if (pthread_mutex_unlock(pmutex) != 0) {
//         return INFRAX_ERROR_MUTEX_UNLOCK;
//     }
//     return INFRAX_OK;
// }

// static InfraxError infrax_core_cond_init(InfraxCore *self, InfraxCond* cond) {
//     pthread_cond_t* pcond = malloc(sizeof(pthread_cond_t));
//     if (!pcond) {
//         return INFRAX_ERROR_OUT_OF_MEMORY;
//     }
    
//     if (pthread_cond_init(pcond, NULL) != 0) {
//         free(pcond);
//         return INFRAX_ERROR_COND_CREATE;
//     }
    
//     *cond = pcond;
//     return INFRAX_OK;
// }

// static void infrax_core_cond_destroy(InfraxCore *self, InfraxCond cond) {
//     pthread_cond_t* pcond = cond;
//     pthread_cond_destroy(pcond);
//     free(pcond);
// }

// static InfraxError infrax_core_cond_wait(InfraxCore *self, InfraxCond cond, InfraxMutex mutex) {
//     pthread_cond_t* pcond = cond;
//     pthread_mutex_t* pmutex = mutex;
//     if (pthread_cond_wait(pcond, pmutex) != 0) {
//         return INFRAX_ERROR_COND_WAIT;
//     }
//     return INFRAX_OK;
// }

// static InfraxError infrax_core_cond_timedwait(InfraxCore *self, InfraxCond cond, InfraxMutex mutex, uint32_t timeout_ms) {
//     pthread_cond_t* pcond = cond;
//     pthread_mutex_t* pmutex = mutex;
    
//     struct timespec ts;
//     clock_gettime(CLOCK_REALTIME, &ts);
//     ts.tv_sec += timeout_ms / 1000;
//     ts.tv_nsec += (timeout_ms % 1000) * 1000000;
//     if (ts.tv_nsec >= 1000000000) {
//         ts.tv_sec++;
//         ts.tv_nsec -= 1000000000;
//     }
    
//     int ret = pthread_cond_timedwait(pcond, pmutex, &ts);
//     if (ret == ETIMEDOUT) {
//         return INFRAX_ERROR_TIMEOUT;
//     } else if (ret != 0) {
//         return INFRAX_ERROR_COND_WAIT;
//     }
//     return INFRAX_OK;
// }

// static InfraxError infrax_core_cond_signal(InfraxCore *self, InfraxCond cond) {
//     pthread_cond_t* pcond = cond;
//     if (pthread_cond_signal(pcond) != 0) {
//         return INFRAX_ERROR_COND_SIGNAL;
//     }
//     return INFRAX_OK;
// }

// static InfraxError infrax_core_cond_broadcast(InfraxCore *self, InfraxCond cond) {
//     pthread_cond_t* pcond = cond;
//     if (pthread_cond_broadcast(pcond) != 0) {
//         return INFRAX_ERROR_COND_SIGNAL;
//     }
//     return INFRAX_OK;
// }

// // Constructor implementation
// InfraxCore* infrax_core_new(void) {
//     struct InfraxCoreImpl* impl = calloc(1, sizeof(struct InfraxCoreImpl));
//     if (!impl) {
//         return NULL;
//     }
    
//     // Initialize interface
//     impl->interface.new = infrax_core_new;
//     impl->interface.free = infrax_core_free;
//     impl->interface.time_now = infrax_core_time_now;
//     impl->interface.time_monotonic = infrax_core_time_monotonic;
//     impl->interface.sleep = infrax_core_sleep;
//     impl->interface.thread_create = infrax_core_thread_create;
//     impl->interface.thread_join = infrax_core_thread_join;
//     impl->interface.mutex_create = infrax_core_mutex_create;
//     impl->interface.mutex_destroy = infrax_core_mutex_destroy;
//     impl->interface.mutex_lock = infrax_core_mutex_lock;
//     impl->interface.mutex_unlock = infrax_core_mutex_unlock;
//     impl->interface.cond_init = infrax_core_cond_init;
//     impl->interface.cond_destroy = infrax_core_cond_destroy;
//     impl->interface.cond_wait = infrax_core_cond_wait;
//     impl->interface.cond_timedwait = infrax_core_cond_timedwait;
//     impl->interface.cond_signal = infrax_core_cond_signal;
//     impl->interface.cond_broadcast = infrax_core_cond_broadcast;
    
//     return &impl->interface;
// }

// // Destructor implementation
// void infrax_core_free(InfraxCore *self) {
//     if (self) {
//         struct InfraxCoreImpl* impl = (struct InfraxCoreImpl*)self;
//         free(impl);
//     }
// }

// Global instance
InfraxCore g_infrax_core = {
    .new_error = infrax_core_new_error,
    .printf = infrax_core_printf,
    .forward_call = infrax_core_forward_call,
    //
    .time_now_ms = infrax_core_time_now_ms,
    .time_monotonic_ms = infrax_core_time_monotonic_ms,
    .sleep_ms = infrax_core_sleep_ms,
};

InfraxCore* get_global_infrax_core(void) {
    // if (!g_infrax_core) {
    //     g_infrax_core = infrax_core_new();
    // }
    // return g_infrax_core;
    return &g_infrax_core;
}
