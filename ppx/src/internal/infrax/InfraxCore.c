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

InfraxU32 infrax_core_random(InfraxCore *self) {
    return rand();  // 使用标准库的 rand 函数
}

void infra_random_seed(InfraxCore *self, uint32_t seed) {
    srand(seed);  // 使用标准库的 srand 函数
}

// Note: PATH_MAX is typically 4096 bytes on Linux/Unix systems
// We should return allocated string to avoid buffer overflow risks
// TODO: Consider changing function signature to:
// char* infrax_get_cwd(InfraxCore *self, InfraxError *error);
// 
// 建议修改方案:
// 1. 改为返回动态分配的字符串,避免缓冲区溢出风险
// 2. 通过 error 参数返回错误信息
// 3. 调用方负责释放返回的字符串
// 4. 实现示例:
//    char* cwd = infrax_get_cwd(core, &error);
//    if(error.code != 0) {
//        // handle error
//    }
//    // use cwd
//    free(cwd);
// 系统路径长度上限:
// Linux/Unix: PATH_MAX 通常是 4096 字节
// Windows: MAX_PATH 通常是 260 字符
// macOS: PATH_MAX 通常是 1024 字节
// 为了兼容性和安全性,我们使用 4096 作为上限
// #define INFRAX_PATH_MAX 4096
InfraxError infrax_get_cwd(InfraxCore *self, char* buffer, size_t size) {
    
    if (!buffer || size == 0) {
        return self->new_error(1, "Invalid buffer or size");
    }

    if (!getcwd(buffer, size)) {
        return self->new_error(2, "Failed to get current working directory");
    }

    return self->new_error(0, NULL);
}
// String operations
static size_t infrax_core_strlen(InfraxCore *self, const char* s) {
    if (!s) return 0;
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static char* infrax_core_strcpy(InfraxCore *self, char* dest, const char* src) {
    if (!dest || !src) return NULL;
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

static char* infrax_core_strncpy(InfraxCore *self, char* dest, const char* src, size_t n) {
    if (!dest || !src || !n) return NULL;
    char* d = dest;
    while (n > 0 && (*d++ = *src++)) n--;
    while (n-- > 0) *d++ = '\0';
    return dest;
}

static char* infrax_core_strcat(InfraxCore *self, char* dest, const char* src) {
    if (!dest || !src) return NULL;
    char* d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

static char* infrax_core_strncat(InfraxCore *self, char* dest, const char* src, size_t n) {
    if (!dest || !src || !n) return NULL;
    char* d = dest;
    while (*d) d++;
    while (n-- > 0 && (*d++ = *src++));
    *d = '\0';
    return dest;
}

static int infrax_core_strcmp(InfraxCore *self, const char* s1, const char* s2) {
    if (!s1 || !s2) return s1 ? 1 : (s2 ? -1 : 0);
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

static int infrax_core_strncmp(InfraxCore *self, const char* s1, const char* s2, size_t n) {
    if (!s1 || !s2 || !n) return 0;
    while (n-- > 0 && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return n < 0 ? 0 : *(unsigned char*)s1 - *(unsigned char*)s2;
}

static char* infrax_core_strchr(InfraxCore *self, const char* s, int c) {
    if (!s) return NULL;
    while (*s && *s != (char)c) s++;
    return *s == (char)c ? (char*)s : NULL;
}

static char* infrax_core_strrchr(InfraxCore *self, const char* s, int c) {
    if (!s) return NULL;
    const char* found = NULL;
    while (*s) {
        if (*s == (char)c) found = s;
        s++;
    }
    if ((char)c == '\0') return (char*)s;
    return (char*)found;
}

static char* infrax_core_strstr(InfraxCore *self, const char* haystack, const char* needle) {
    if (!haystack || !needle) return NULL;
    if (!*needle) return (char*)haystack;
    
    char* h = (char*)haystack;
    while (*h) {
        char* h1 = h;
        const char* n = needle;
        while (*h1 && *n && *h1 == *n) {
            h1++;
            n++;
        }
        if (!*n) return h;
        h++;
    }
    return NULL;
}

static char* infrax_core_strdup(InfraxCore *self, const char* s) {
    if (!s) return NULL;
    size_t len = infrax_core_strlen(self, s) + 1;
    char* new_str = malloc(len);
    if (new_str) {
        infrax_core_strcpy(self, new_str, s);
    }
    return new_str;
}

static char* infrax_core_strndup(InfraxCore *self, const char* s, size_t n) {
    if (!s) return NULL;
    size_t len = infrax_core_strlen(self, s);
    if (len > n) len = n;
    char* new_str = malloc(len + 1);
    if (new_str) {
        infrax_core_strncpy(self, new_str, s, len);
        new_str[len] = '\0';
    }
    return new_str;
}

/* TODO
//-----------------------------------------------------------------------------
// File Operations
//-----------------------------------------------------------------------------

#define INFRA_FILE_CREATE  (1 << 0)
#define INFRA_FILE_RDONLY  (1 << 1)
#define INFRA_FILE_WRONLY  (1 << 2)
#define INFRA_FILE_RDWR    (INFRA_FILE_RDONLY | INFRA_FILE_WRONLY)
#define INFRA_FILE_APPEND  (1 << 3)
#define INFRA_FILE_TRUNC   (1 << 4)

#define INFRA_SEEK_SET 0
#define INFRA_SEEK_CUR 1
#define INFRA_SEEK_END 2

infra_error_t infra_file_open(const char* path, infra_flags_t flags, int mode, INFRA_CORE_Handle_t* handle);
infra_error_t infra_file_close(INFRA_CORE_Handle_t handle);
infra_error_t infra_file_read(INFRA_CORE_Handle_t handle, void* buffer, size_t size, size_t* bytes_read);
infra_error_t infra_file_write(INFRA_CORE_Handle_t handle, const void* buffer, size_t size, size_t* bytes_written);
infra_error_t infra_file_seek(INFRA_CORE_Handle_t handle, int64_t offset, int whence);
infra_error_t infra_file_size(INFRA_CORE_Handle_t handle, size_t* size);
infra_error_t infra_file_remove(const char* path);
infra_error_t infra_file_rename(const char* old_path, const char* new_path);
infra_error_t infra_file_exists(const char* path, bool* exists);
*/

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
