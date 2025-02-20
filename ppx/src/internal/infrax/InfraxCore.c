#include "cosmopolitan.h"
#include "InfraxCore.h"

//forward declare
// Network operations
typedef intptr_t (*InfraxCoreSocketCreate)(struct InfraxCore* self, int domain, int type, int protocol);
typedef int (*InfraxCoreSocketBind)(struct InfraxCore* self, intptr_t handle, const void* addr, size_t size);
typedef int (*InfraxCoreSocketListen)(struct InfraxCore* self, intptr_t handle, int backlog);
typedef intptr_t (*InfraxCoreSocketAccept)(struct InfraxCore* self, intptr_t handle, void* addr, size_t* size);
typedef int (*InfraxCoreSocketConnect)(struct InfraxCore* self, intptr_t handle, const void* addr, size_t size);
typedef ssize_t (*InfraxCoreSocketSend)(struct InfraxCore* self, intptr_t handle, const void* data, size_t size, int flags);
typedef ssize_t (*InfraxCoreSocketRecv)(struct InfraxCore* self, intptr_t handle, void* buffer, size_t size, int flags);
typedef int (*InfraxCoreSocketClose)(struct InfraxCore* self, intptr_t handle);
typedef int (*InfraxCoreSocketShutdown)(struct InfraxCore* self, intptr_t handle, int how);
typedef int (*InfraxCoreSocketSetOption)(struct InfraxCore* self, intptr_t handle, int level, int option, const void* value, size_t len);
typedef int (*InfraxCoreSocketGetOption)(struct InfraxCore* self, intptr_t handle, int level, int option, void* value, size_t* len);
typedef int (*InfraxCoreSocketGetError)(struct InfraxCore* self, intptr_t handle);

// Network address operations
typedef int (*InfraxCoreIpToBinary)(struct InfraxCore* self, const char* ip, void* addr, size_t size);
typedef int (*InfraxCoreBinaryToIp)(struct InfraxCore* self, const void* addr, char* ip, size_t size);

// Error handling
typedef int (*InfraxCoreGetLastError)(struct InfraxCore* self);
typedef const char* (*InfraxCoreGetErrorString)(struct InfraxCore* self, int error_code);



void* infrax_core_malloc(InfraxCore *self, size_t size) {
    return malloc(size);
}

void* infrax_core_calloc(InfraxCore *self, size_t nmemb, size_t size) {
    return calloc(nmemb, size);
}

void* infrax_core_realloc(InfraxCore *self, void* ptr, size_t size) {
    return realloc(ptr, size);
}

void infrax_core_free(InfraxCore *self, void* ptr) {
    free(ptr);
}
int infrax_core_printf(InfraxCore *self, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);
    return result;
}

int infrax_core_fprintf(InfraxCore *self, int* stream, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = fprintf((FILE*)stream, format, args);
    va_end(args);
    return result;
}

int infrax_core_snprintf(InfraxCore *self, char* str, size_t size, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(str, size, format, args);
    va_end(args);
}
// Parameter forwarding function implementation
void* infrax_core_forward_call(InfraxCore *self,void* (*target_func)(va_list), ...) {
    va_list args;
    va_start(args, target_func);
    void* result = target_func(args);
    va_end(args);
    return result;
}

static void infrax_core_hint_yield(InfraxCore *self) {
    sched_yield();//在多线程情况下，提示当前线程放弃CPU，让其他线程运行，但并不是一定成功的
}

int infrax_core_pid(InfraxCore *self) {
    return getpid();
}

// Network byte order conversion implementations
static uint16_t infrax_core_host_to_net16(InfraxCore *self, uint16_t host16) {
    return htons(host16);
}

static uint32_t infrax_core_host_to_net32(InfraxCore *self, uint32_t host32) {
    return htonl(host32);
}

static uint64_t infrax_core_host_to_net64(InfraxCore *self, uint64_t host64) {
    // htonll is not standard on all platforms, so we implement it
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        return ((uint64_t)htonl((uint32_t)host64) << 32) | htonl((uint32_t)(host64 >> 32));
    #else
        return host64;
    #endif
}

static uint16_t infrax_core_net_to_host16(InfraxCore *self, uint16_t net16) {
    return ntohs(net16);
}

static uint32_t infrax_core_net_to_host32(InfraxCore *self, uint32_t net32) {
    return ntohl(net32);
}

static uint64_t infrax_core_net_to_host64(InfraxCore *self, uint64_t net64) {
    // ntohll is not standard on all platforms, so we implement it
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        return ((uint64_t)ntohl((uint32_t)net64) << 32) | ntohl((uint32_t)(net64 >> 32));
    #else
        return net64;
    #endif
}

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

static InfraxU32 infrax_core_random(InfraxCore *self) {
    return rand();  // 使用标准库的 rand 函数
}

static void infrax_core_random_seed(InfraxCore *self, uint32_t seed) {
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
        return make_error(1, "Invalid buffer or size");
    }

    if (!getcwd(buffer, size)) {
        return make_error(2, "Failed to get current working directory");
    }

    return make_error(0, NULL);
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

//-----------------------------------------------------------------------------
// Buffer Operations
//-----------------------------------------------------------------------------

static InfraxError infrax_core_buffer_init(InfraxCore *self, InfraxBuffer* buf, size_t initial_capacity) {
    if (!buf || initial_capacity == 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid buffer parameters");
    }
    
    buf->data = (uint8_t*)malloc(initial_capacity);
    if (!buf->data) {
        return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate buffer memory");
    }
    
    buf->size = 0;
    buf->capacity = initial_capacity;
    return INFRAX_ERROR_OK_STRUCT;
}

static void infrax_core_buffer_destroy(InfraxCore *self, InfraxBuffer* buf) {
    if (buf && buf->data) {
        free(buf->data);
        buf->data = NULL;
        buf->size = 0;
        buf->capacity = 0;
    }
}

static InfraxError infrax_core_buffer_reserve(InfraxCore *self, InfraxBuffer* buf, size_t capacity) {
    if (!buf || !buf->data) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid buffer");
    }
    
    if (capacity <= buf->capacity) {
        return INFRAX_ERROR_OK_STRUCT;
    }
    
    uint8_t* new_data = (uint8_t*)realloc(buf->data, capacity);
    if (!new_data) {
        return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to reallocate buffer memory");
    }
    
    buf->data = new_data;
    buf->capacity = capacity;
    return INFRAX_ERROR_OK_STRUCT;
}
static int infrax_core_memcmp(InfraxCore *self, const void* ptr1, const void* ptr2, size_t num) {
    if (!ptr1 || !ptr2) {
        return 0;
    }
    return memcmp(ptr1, ptr2, num);
}

static void* infrax_core_memset(InfraxCore *self, void* ptr, int value, size_t num) {
    if (!ptr || num == 0) return ptr;
    return memset(ptr, value, num);
}

static InfraxError infrax_core_buffer_write(InfraxCore *self, InfraxBuffer* buf, const void* data, size_t size) {
    if (!buf || !buf->data || !data || size == 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid buffer write parameters");
    }
    
    if (buf->size + size > buf->capacity) {
        InfraxError err = infrax_core_buffer_reserve(self, buf, (buf->size + size) * 2);
        if (INFRAX_ERROR_IS_ERR(err)) {
            return err;
        }
    }
    
    memcpy(buf->data + buf->size, data, size);
    buf->size += size;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError infrax_core_buffer_read(InfraxCore *self, InfraxBuffer* buf, void* data, size_t size) {
    if (!buf || !buf->data || !data || size == 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid buffer read parameters");
    }
    
    if (size > buf->size) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Read size exceeds buffer size");
    }
    
    memcpy(data, buf->data, size);
    memmove(buf->data, buf->data + size, buf->size - size);
    buf->size -= size;
    return INFRAX_ERROR_OK_STRUCT;
}

static size_t infrax_core_buffer_readable(InfraxCore *self, const InfraxBuffer* buf) {
    return buf ? buf->size : 0;
}

static size_t infrax_core_buffer_writable(InfraxCore *self, const InfraxBuffer* buf) {
    return buf ? (buf->capacity - buf->size) : 0;
}

static void infrax_core_buffer_reset(InfraxCore *self, InfraxBuffer* buf) {
    if (buf) {
        buf->size = 0;
    }
}

//-----------------------------------------------------------------------------
// Ring Buffer Operations
//-----------------------------------------------------------------------------

static size_t infrax_core_ring_buffer_readable(InfraxCore *self, const InfraxRingBuffer* rb) {
    if (!rb) return 0;
    if (rb->full) return rb->size;
    if (rb->write_pos >= rb->read_pos) {
        return rb->write_pos - rb->read_pos;
    }
    return rb->size - (rb->read_pos - rb->write_pos);
}

static size_t infrax_core_ring_buffer_writable(InfraxCore *self, const InfraxRingBuffer* rb) {
    if (!rb) return 0;
    if (rb->full) return 0;
    return rb->size - infrax_core_ring_buffer_readable(self, rb);
}

static InfraxError infrax_core_ring_buffer_init(InfraxCore *self, InfraxRingBuffer* rb, size_t size) {
    if (!rb || size == 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid ring buffer parameters");
    }
    
    rb->buffer = (uint8_t*)malloc(size);
    if (!rb->buffer) {
        return make_error(INFRAX_ERROR_NO_MEMORY, "Failed to allocate ring buffer memory");
    }
    
    rb->size = size;
    rb->read_pos = 0;
    rb->write_pos = 0;
    rb->full = INFRAX_FALSE;
    return INFRAX_ERROR_OK_STRUCT;
}

static void infrax_core_ring_buffer_destroy(InfraxCore *self, InfraxRingBuffer* rb) {
    if (rb && rb->buffer) {
        free(rb->buffer);
        rb->buffer = NULL;
        rb->size = 0;
        rb->read_pos = 0;
        rb->write_pos = 0;
        rb->full = INFRAX_FALSE;
    }
}

static InfraxError infrax_core_ring_buffer_write(InfraxCore *self, InfraxRingBuffer* rb, const void* data, size_t size) {
    if (!rb || !rb->buffer || !data || size == 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid ring buffer write parameters");
    }
    
    if (size > rb->size) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Write size exceeds ring buffer size");
    }
    
    size_t available = infrax_core_ring_buffer_writable(self, rb);
    if (size > available) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Not enough space in ring buffer");
    }
    
    const uint8_t* src = (const uint8_t*)data;
    size_t first_chunk = rb->size - rb->write_pos;
    if (size <= first_chunk) {
        memcpy(rb->buffer + rb->write_pos, src, size);
    } else {
        memcpy(rb->buffer + rb->write_pos, src, first_chunk);
        memcpy(rb->buffer, src + first_chunk, size - first_chunk);
    }
    
    rb->write_pos = (rb->write_pos + size) % rb->size;
    rb->full = (rb->write_pos == rb->read_pos);
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError infrax_core_ring_buffer_read(InfraxCore *self, InfraxRingBuffer* rb, void* data, size_t size) {
    if (!rb || !rb->buffer || !data || size == 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid ring buffer read parameters");
    }
    
    size_t available = infrax_core_ring_buffer_readable(self, rb);
    if (size > available) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Not enough data in ring buffer");
    }
    
    uint8_t* dst = (uint8_t*)data;
    size_t first_chunk = rb->size - rb->read_pos;
    if (size <= first_chunk) {
        memcpy(dst, rb->buffer + rb->read_pos, size);
    } else {
        memcpy(dst, rb->buffer + rb->read_pos, first_chunk);
        memcpy(dst + first_chunk, rb->buffer, size - first_chunk);
    }
    
    rb->read_pos = (rb->read_pos + size) % rb->size;
    rb->full = INFRAX_FALSE;
    return INFRAX_ERROR_OK_STRUCT;
}

static void infrax_core_ring_buffer_reset(InfraxCore *self, InfraxRingBuffer* rb) {
    if (rb) {
        rb->read_pos = 0;
        rb->write_pos = 0;
        rb->full = INFRAX_FALSE;
    }
}

//-----------------------------------------------------------------------------
// File Operations
//-----------------------------------------------------------------------------

static InfraxError infrax_core_file_open(InfraxCore *self, const char* path, InfraxFlags flags, int mode, InfraxHandle* handle) {
    if (!path || !handle) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid file parameters");
    }
    
    int oflags = 0;
    if (flags & INFRAX_FILE_CREATE) oflags |= O_CREAT;
    if (flags & INFRAX_FILE_RDONLY) oflags |= O_RDONLY;
    if (flags & INFRAX_FILE_WRONLY) oflags |= O_WRONLY;
    if (flags & INFRAX_FILE_RDWR) oflags |= O_RDWR;
    if (flags & INFRAX_FILE_APPEND) oflags |= O_APPEND;
    if (flags & INFRAX_FILE_TRUNC) oflags |= O_TRUNC;
    
    // 确保文件目录存在
    char dir[PATH_MAX];
    strncpy(dir, path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';
    char* last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir(dir, 0755);
    }
    
    // 如果是只读模式，需要检查文件是否存在
    if ((flags & INFRAX_FILE_RDONLY) && !(flags & INFRAX_FILE_CREATE)) {
        int exists = access(path, F_OK);
        if (exists != 0) {
            char err_msg[256];
            snprintf(err_msg, sizeof(err_msg), "File '%s' does not exist", path);
            return make_error(INFRAX_ERROR_INVALID_PARAM, err_msg);
        }
    }
    
    // 打开文件
    int fd = open(path, oflags, mode);
    if (fd < 0) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Failed to open file '%s': %s", path, strerror(errno));
        return make_error(INFRAX_ERROR_INVALID_PARAM, err_msg);
    }
    
    *handle = (InfraxHandle)fd;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError infrax_core_file_close(InfraxCore *self, InfraxHandle handle) {
    if (close((int)handle) < 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Failed to close file");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError infrax_core_file_read(InfraxCore *self, InfraxHandle handle, void* buffer, size_t size, size_t* bytes_read) {
    if (!buffer || !bytes_read) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid read parameters");
    }
    
    ssize_t result = read((int)handle, buffer, size);
    if (result < 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Failed to read from file");
    }
    
    *bytes_read = (size_t)result;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError infrax_core_file_write(InfraxCore *self, InfraxHandle handle, const void* buffer, size_t size, size_t* bytes_written) {
    if (!buffer || !bytes_written) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid write parameters");
    }
    
    ssize_t result = write((int)handle, buffer, size);
    if (result < 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Failed to write to file");
    }
    
    *bytes_written = (size_t)result;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError infrax_core_file_seek(InfraxCore *self, InfraxHandle handle, int64_t offset, int whence) {
    if (lseek((int)handle, offset, whence) < 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Failed to seek in file");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError infrax_core_file_size(InfraxCore *self, InfraxHandle handle, size_t* size) {
    if (!size) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid size parameter");
    }
    
    struct stat st;
    if (fstat((int)handle, &st) < 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Failed to get file size");
    }
    
    *size = (size_t)st.st_size;
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError infrax_core_file_remove(InfraxCore *self, const char* path) {
    if (!path) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid path parameter");
    }
    
    if (unlink(path) < 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Failed to remove file");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError infrax_core_file_rename(InfraxCore *self, const char* old_path, const char* new_path) {
    if (!old_path || !new_path) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid path parameters");
    }
    
    if (rename(old_path, new_path) < 0) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Failed to rename file");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError infrax_core_file_exists(InfraxCore *self, const char* path, InfraxBool* exists) {
    if (!path || !exists) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }
    
    struct stat st;
    *exists = (stat(path, &st) == 0) ? INFRAX_TRUE : INFRAX_FALSE;
    return INFRAX_ERROR_OK_STRUCT;
}

// Default assert handler
static InfraxAssertHandler g_assert_handler = NULL;

static void default_assert_handler(const char* file, int line, const char* func, const char* expr, const char* msg) {
    fprintf(stderr, "Assertion failed at %s:%d in %s\n", file, line, func);
    fprintf(stderr, "Expression: %s\n", expr);
    if (msg) {
        fprintf(stderr, "Message: %s\n", msg);
    }
    abort();
}

static void infrax_core_assert_failed(InfraxCore *self, const char* file, int line, const char* func, const char* expr, const char* msg) {
    if (g_assert_handler) {
        g_assert_handler(file, line, func, expr, msg);
    } else {
        default_assert_handler(file, line, func, expr, msg);
    }
}

static void infrax_core_set_assert_handler(InfraxCore *self, InfraxAssertHandler handler) {
    g_assert_handler = handler;
}

// File descriptor operations
static ssize_t infrax_core_read_fd(InfraxCore *self, int fd, void* buf, size_t count) {
    return read(fd, buf, count);
}

static ssize_t infrax_core_write_fd(InfraxCore *self, int fd, const void* buf, size_t count) {
    return write(fd, buf, count);
}

static int infrax_core_create_pipe(InfraxCore *self, int pipefd[2]) {
    return pipe(pipefd);
}

static int infrax_core_set_nonblocking(InfraxCore *self, int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int infrax_core_close_fd(InfraxCore *self, int fd) {
    return close(fd);
}

// Time operations
static InfraxTime* infrax_core_localtime(InfraxCore *self, InfraxTime* tloc) {
    if (!tloc) return NULL;
    time_t t = (time_t)*tloc;
    struct tm* tm_info = localtime(&t);
    if (!tm_info) return NULL;
    *tloc = (InfraxTime)mktime(tm_info);
    return tloc;
}

static InfraxClock infrax_core_clock(InfraxCore *self) {
    return clock();
}

static int infrax_core_clock_gettime(InfraxCore *self, int clk_id, InfraxTimeSpec* tp) {
    struct timespec ts;
    int result = clock_gettime(clk_id == INFRAX_CLOCK_REALTIME ? CLOCK_REALTIME : CLOCK_MONOTONIC, &ts);
    if (result == 0) {
        tp->tv_sec = ts.tv_sec;
        tp->tv_nsec = ts.tv_nsec;
    }
    return result;
}

static InfraxTime infrax_core_time(InfraxCore *self, InfraxTime* tloc) {
    time_t t = time(NULL);
    if (tloc) {
        *tloc = (InfraxTime)t;
    }
    return (InfraxTime)t;
}

static int infrax_core_clocks_per_sec(InfraxCore *self) {
    return CLOCKS_PER_SEC;
}

static void infrax_core_sleep(InfraxCore *self, unsigned int seconds) {
    sleep(seconds);
}

static void infrax_core_sleep_us(InfraxCore *self, unsigned int microseconds) {
    usleep(microseconds);
}

static size_t infrax_core_get_memory_usage(InfraxCore *self) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;
}

static InfraxSignalHandler infrax_core_signal(InfraxCore *self, int signum, InfraxSignalHandler handler) {
    return signal(signum, handler);
}

static unsigned int infrax_core_alarm(InfraxCore *self, unsigned int seconds) {
    return alarm(seconds);
}

static void* infrax_core_memcpy(InfraxCore *self, void* dest, const void* src, size_t n) {
    if (!dest || !src || !n) return dest;
    unsigned char* d = dest;
    const unsigned char* s = src;
    while (n--) *d++ = *s++;
    return dest;
}

static void* infrax_core_memmove(InfraxCore *self, void* dest, const void* src, size_t n) {
    if (!dest || !src || !n) return dest;
    unsigned char* d = dest;
    const unsigned char* s = src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

static int infrax_core_isspace(InfraxCore *self, int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int infrax_core_isdigit(InfraxCore *self, int c) {
    return c >= '0' && c <= '9';
}

// String to number conversion
static int infrax_core_atoi(InfraxCore *self, const char* str) {
    if (!str) return 0;
    
    int result = 0;
    int sign = 1;
    
    // Skip leading whitespace
    while (infrax_core_isspace(self, *str)) str++;
    
    // Handle sign
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    // Convert digits
    while (infrax_core_isdigit(self, *str)) {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
}

static long infrax_core_atol(InfraxCore *self, const char* str) {
    if (!str) return 0;
    
    long result = 0;
    int sign = 1;
    
    // Skip leading whitespace
    while (infrax_core_isspace(self, *str)) str++;
    
    // Handle sign
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    // Convert digits
    while (infrax_core_isdigit(self, *str)) {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
}

static long long infrax_core_atoll(InfraxCore *self, const char* str) {
    if (!str) return 0;
    
    long long result = 0;
    int sign = 1;
    
    // Skip leading whitespace
    while (infrax_core_isspace(self, *str)) str++;
    
    // Handle sign
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    // Convert digits
    while (infrax_core_isdigit(self, *str)) {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
}

// Network operations implementations
static intptr_t infrax_core_socket_create(InfraxCore* self, int domain, int type, int protocol) {
    // Map domain
    int sys_domain;
    switch (domain) {
        case INFRAX_AF_INET: sys_domain = AF_INET; break;
        default: return -1;
    }
    
    // Map type
    int sys_type;
    switch (type) {
        case INFRAX_SOCK_STREAM: sys_type = SOCK_STREAM; break;
        case INFRAX_SOCK_DGRAM: sys_type = SOCK_DGRAM; break;
        default: return -1;
    }
    
    // Create socket
    intptr_t fd = socket(sys_domain, sys_type, protocol);
    if (fd < 0) {
        printf("Failed to create socket: %s\n", strerror(errno));
    }
    return fd;
}

static int infrax_core_socket_bind(InfraxCore* self, intptr_t handle, const void* addr, size_t size) {
    return bind(handle, (const struct sockaddr*)addr, size);
}

static int infrax_core_socket_listen(InfraxCore* self, intptr_t handle, int backlog) {
    return listen(handle, backlog);
}

static intptr_t infrax_core_socket_accept(InfraxCore* self, intptr_t handle, void* addr, size_t* size) {
    socklen_t addr_len = *size;
    intptr_t result = accept(handle, (struct sockaddr*)addr, &addr_len);
    *size = addr_len;
    return result;
}

static int infrax_core_socket_connect(InfraxCore* self, intptr_t handle, const void* addr, size_t size) {
    return connect(handle, (const struct sockaddr*)addr, size);
}

static ssize_t infrax_core_socket_send(InfraxCore* self, intptr_t handle, const void* data, size_t size, int flags) {
    return send(handle, data, size, flags);
}

static ssize_t infrax_core_socket_recv(InfraxCore* self, intptr_t handle, void* buffer, size_t size, int flags) {
    return recv(handle, buffer, size, flags);
}

static int infrax_core_socket_close(InfraxCore* self, intptr_t handle) {
    return close(handle);
}

static int infrax_core_socket_shutdown(InfraxCore* self, intptr_t handle, int how) {
    return shutdown(handle, how);
}

static int infrax_core_socket_set_option(InfraxCore* self, intptr_t handle, int level, int option, const void* value, size_t len) {
    return setsockopt(handle, level, option, value, len);
}

static int infrax_core_socket_get_option(InfraxCore* self, intptr_t handle, int level, int option, void* value, size_t* len) {
    socklen_t optlen = *len;
    int result = getsockopt(handle, level, option, value, &optlen);
    *len = optlen;
    return result;
}

static int infrax_core_socket_get_error(InfraxCore* self, intptr_t handle) {
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(handle, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
        error = errno;
    }
    if (error == 0) {
        error = errno;  // 如果 SO_ERROR 为 0，使用当前的 errno
    }
    return error;
}

// Network address operations implementations
static int infrax_core_ip_to_binary(InfraxCore* self, const char* ip, void* addr, size_t size) {
    return inet_pton(AF_INET, ip, addr);
}

static int infrax_core_binary_to_ip(InfraxCore* self, const void* addr, char* ip, size_t size) {
    return inet_ntop(AF_INET, addr, ip, size) ? 0 : -1;
}

// Error handling implementations
static int infrax_core_get_last_error(InfraxCore* self) {
    return errno;
}

static const char* infrax_core_get_error_string(InfraxCore* self, int error_code) {
    return strerror(error_code);
}

static int infrax_core_socket_get_name(InfraxCore* self, intptr_t handle, void* addr, size_t* addr_len) {
    return getsockname(handle, (struct sockaddr*)addr, (socklen_t*)addr_len);
}

static int infrax_core_socket_get_peer(InfraxCore* self, intptr_t handle, void* addr, size_t* addr_len) {
    return getpeername(handle, (struct sockaddr*)addr, (socklen_t*)addr_len);
}

// File descriptor set operations
static void infrax_core_fd_zero(InfraxCore* self, InfraxFdSet* set) {
    if (!set) return;
    memset(set->fds_bits, 0, sizeof(set->fds_bits));
}

static void infrax_core_fd_set(InfraxCore* self, int fd, InfraxFdSet* set) {
    if (!set || fd < 0 || fd >= INFRAX_FD_SETSIZE) return;
    set->fds_bits[fd / (8 * sizeof(unsigned long))] |= (1UL << (fd % (8 * sizeof(unsigned long))));
}

static void infrax_core_fd_clr(InfraxCore* self, int fd, InfraxFdSet* set) {
    if (!set || fd < 0 || fd >= INFRAX_FD_SETSIZE) return;
    set->fds_bits[fd / (8 * sizeof(unsigned long))] &= ~(1UL << (fd % (8 * sizeof(unsigned long))));
}

static int infrax_core_fd_isset(InfraxCore* self, int fd, InfraxFdSet* set) {
    if (!set || fd < 0 || fd >= INFRAX_FD_SETSIZE) return 0;
    return !!(set->fds_bits[fd / (8 * sizeof(unsigned long))] & (1UL << (fd % (8 * sizeof(unsigned long)))));
}

// File descriptor control operations
static int infrax_core_fcntl(InfraxCore* self, int fd, int cmd, int arg) {
    if (fd < 0) return -1;
    return fcntl(fd, cmd, arg);
}

// IO multiplexing
static int infrax_core_select(InfraxCore* self, int nfds, InfraxFdSet* readfds, 
                            InfraxFdSet* writefds, InfraxFdSet* exceptfds, 
                            InfraxTimeVal* timeout) {
    if (nfds < 0 || nfds > INFRAX_FD_SETSIZE) return -1;
    
    fd_set* r = (fd_set*)readfds;
    fd_set* w = (fd_set*)writefds;
    fd_set* e = (fd_set*)exceptfds;
    struct timeval* t = (struct timeval*)timeout;
    
    return select(nfds, r, w, e, t);
}

// High-level IO operations
static InfraxError infrax_core_wait_for_read(InfraxCore* self, int fd, int timeout_ms) {
    if (fd < 0) return make_error(INFRAX_ERROR_IO_INVALID_FD, "Invalid file descriptor");
    
    InfraxFdSet readfds;
    InfraxTimeVal tv;
    
    infrax_core_fd_zero(self, &readfds);
    infrax_core_fd_set(self, fd, &readfds);
    
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int result = infrax_core_select(self, fd + 1, &readfds, NULL, NULL, &tv);
    if (result < 0) {
        if (errno == EINTR) {
            return make_error(INFRAX_ERROR_IO_INTERRUPTED, "Operation interrupted");
        }
        return make_error(INFRAX_ERROR_IO_SELECT_FAILED, "Select failed");
    }
    if (result == 0) {
        return make_error(INFRAX_ERROR_IO_TIMEOUT, "Operation timed out");
    }
    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError infrax_core_wait_for_write(InfraxCore* self, int fd, int timeout_ms) {
    if (fd < 0) return make_error(INFRAX_ERROR_IO_INVALID_FD, "Invalid file descriptor");
    
    InfraxFdSet writefds;
    InfraxTimeVal tv;
    
    infrax_core_fd_zero(self, &writefds);
    infrax_core_fd_set(self, fd, &writefds);
    
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int result = infrax_core_select(self, fd + 1, NULL, &writefds, NULL, &tv);
    if (result < 0) {
        if (errno == EINTR) {
            return make_error(INFRAX_ERROR_IO_INTERRUPTED, "Operation interrupted");
        }
        return make_error(INFRAX_ERROR_IO_SELECT_FAILED, "Select failed");
    }
    if (result == 0) {
        return make_error(INFRAX_ERROR_IO_TIMEOUT, "Operation timed out");
    }
    return make_error(INFRAX_ERROR_OK, NULL);
}

static InfraxError infrax_core_wait_for_except(InfraxCore* self, int fd, int timeout_ms) {
    if (fd < 0) return make_error(INFRAX_ERROR_IO_INVALID_FD, "Invalid file descriptor");
    
    InfraxFdSet exceptfds;
    InfraxTimeVal tv;
    
    infrax_core_fd_zero(self, &exceptfds);
    infrax_core_fd_set(self, fd, &exceptfds);
    
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int result = infrax_core_select(self, fd + 1, NULL, NULL, &exceptfds, &tv);
    if (result < 0) {
        if (errno == EINTR) {
            return make_error(INFRAX_ERROR_IO_INTERRUPTED, "Operation interrupted");
        }
        return make_error(INFRAX_ERROR_IO_SELECT_FAILED, "Select failed");
    }
    if (result == 0) {
        return make_error(INFRAX_ERROR_IO_TIMEOUT, "Operation timed out");
    }
    return make_error(INFRAX_ERROR_OK, NULL);
}

//-----------------------------------------------------------------------------
// Timer System
//-----------------------------------------------------------------------------

#define INITIAL_TIMER_CAPACITY 1024
#define INVALID_TIMER_ID 0

// Timer structure
typedef struct {
    InfraxU32 id;
    uint64_t expire_time;
    InfraxU32 interval_ms;
    InfraxBool is_interval;
    InfraxBool is_valid;
    InfraxBool is_paused;
    void (*handler)(void*);
    void* arg;
} InfraxTimer;

// Timer system
typedef struct {
    InfraxTimer* timers;     // Dynamic timer pool
    InfraxTimer** heap;      // Dynamic min heap
    size_t capacity;         // Current capacity
    size_t heap_size;        // Current heap size
    InfraxU32 next_id;
    InfraxBool initialized;
} InfraxTimerSystem;

static InfraxTimerSystem g_timers = {0};

// Timer system helper functions
static void heap_swap(size_t i, size_t j) {
    InfraxTimer* temp = g_timers.heap[i];
    g_timers.heap[i] = g_timers.heap[j];
    g_timers.heap[j] = temp;
}

static void heap_up(size_t pos) {
    while (pos > 0) {
        size_t parent = (pos - 1) / 2;
        if (g_timers.heap[parent]->expire_time <= g_timers.heap[pos]->expire_time) {
            break;
        }
        heap_swap(parent, pos);
        pos = parent;
    }
}

static void heap_down(size_t pos) {
    while (true) {
        size_t min_pos = pos;
        size_t left = 2 * pos + 1;
        size_t right = 2 * pos + 2;
        
        if (left < g_timers.heap_size && 
            g_timers.heap[left]->expire_time < g_timers.heap[min_pos]->expire_time) {
            min_pos = left;
        }
        
        if (right < g_timers.heap_size && 
            g_timers.heap[right]->expire_time < g_timers.heap[min_pos]->expire_time) {
            min_pos = right;
        }
        
        if (min_pos == pos) break;
        
        heap_swap(pos, min_pos);
        pos = min_pos;
    }
}

static void remove_timer_from_heap(InfraxTimer* timer) {
    if (!timer || g_timers.heap_size == 0) return;
    
    // Find timer in heap
    size_t pos;
    for (pos = 0; pos < g_timers.heap_size; pos++) {
        if (g_timers.heap[pos] == timer) break;
    }
    
    if (pos == g_timers.heap_size) return;  // Not found
    
    // Replace with last element and remove last
    g_timers.heap[pos] = g_timers.heap[--g_timers.heap_size];
    
    // Restore heap property
    if (pos > 0 && g_timers.heap[pos]->expire_time < g_timers.heap[(pos-1)/2]->expire_time) {
        heap_up(pos);
    } else {
        heap_down(pos);
    }
}

static InfraxBool init_timer_system(void) {
    if (g_timers.initialized) return true;
    
    // Allocate initial arrays
    g_timers.timers = (InfraxTimer*)malloc(INITIAL_TIMER_CAPACITY * sizeof(InfraxTimer));
    g_timers.heap = (InfraxTimer**)malloc(INITIAL_TIMER_CAPACITY * sizeof(InfraxTimer*));
    
    if (!g_timers.timers || !g_timers.heap) {
        if (g_timers.timers) free(g_timers.timers);
        if (g_timers.heap) free(g_timers.heap);
        return false;
    }
    
    g_timers.capacity = INITIAL_TIMER_CAPACITY;
    g_timers.heap_size = 0;
    g_timers.next_id = 1;  // Start from 1
    g_timers.initialized = true;
    
    return true;
}

static InfraxBool expand_timer_arrays(void) {
    size_t new_capacity = g_timers.capacity * 2;
    
    // Allocate new arrays
    InfraxTimer* new_timers = (InfraxTimer*)malloc(new_capacity * sizeof(InfraxTimer));
    InfraxTimer** new_heap = (InfraxTimer**)malloc(new_capacity * sizeof(InfraxTimer*));
    
    if (!new_timers || !new_heap) {
        if (new_timers) free(new_timers);
        if (new_heap) free(new_heap);
        return false;
    }
    
    // Copy existing data
    memcpy(new_timers, g_timers.timers, g_timers.capacity * sizeof(InfraxTimer));
    memcpy(new_heap, g_timers.heap, g_timers.heap_size * sizeof(InfraxTimer*));
    
    // Update heap pointers
    for (size_t i = 0; i < g_timers.heap_size; i++) {
        size_t offset = g_timers.heap[i] - g_timers.timers;
        new_heap[i] = new_timers + offset;
    }
    
    // Free old arrays
    free(g_timers.timers);
    free(g_timers.heap);
    
    // Update system state
    g_timers.timers = new_timers;
    g_timers.heap = new_heap;
    g_timers.capacity = new_capacity;
    
    return true;
}

static InfraxBool add_timer_to_heap(InfraxTimer* timer) {
    if (!timer || !timer->is_valid || timer->is_paused) return false;
    
    // Check if expansion is needed
    if (g_timers.heap_size >= g_timers.capacity) {
        if (!expand_timer_arrays()) return false;
    }
    
    size_t pos = g_timers.heap_size++;
    g_timers.heap[pos] = timer;
    heap_up(pos);
    
    return true;
}

// Timer interface implementations
static InfraxU32 infrax_timer_create(struct InfraxCore* self, InfraxU32 interval_ms, InfraxBool is_interval, void (*handler)(void*), void* arg) {
    if (!init_timer_system() || !handler) return INVALID_TIMER_ID;
    
    // Find free timer slot
    InfraxTimer* timer = NULL;
    for (size_t i = 0; i < g_timers.capacity; i++) {
        if (!g_timers.timers[i].is_valid) {
            timer = &g_timers.timers[i];
            break;
        }
    }
    
    // If no free slot, try to expand
    if (!timer) {
        if (!expand_timer_arrays()) return INVALID_TIMER_ID;
        timer = &g_timers.timers[g_timers.capacity / 2];  // Use first slot in new space
    }
    
    // Initialize timer
    timer->id = g_timers.next_id++;
    timer->expire_time = self->time_monotonic_ms(self) + interval_ms;
    timer->interval_ms = interval_ms;
    timer->is_interval = is_interval;
    timer->is_valid = true;
    timer->is_paused = false;
    timer->handler = handler;
    timer->arg = arg;
    
    // Add to heap
    if (!add_timer_to_heap(timer)) {
        timer->is_valid = false;
        return INVALID_TIMER_ID;
    }
    
    return timer->id;
}

static InfraxError infrax_timer_delete(struct InfraxCore* self, InfraxU32 timer_id) {
    if (!g_timers.initialized || timer_id == INVALID_TIMER_ID) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid timer ID or timer system not initialized");
    }
    
    // Find timer
    InfraxTimer* timer = NULL;
    for (size_t i = 0; i < g_timers.capacity; i++) {
        if (g_timers.timers[i].is_valid && g_timers.timers[i].id == timer_id) {
            timer = &g_timers.timers[i];
            break;
        }
    }
    
    if (!timer) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Timer not found");
    }
    
    // Remove from heap and invalidate
    remove_timer_from_heap(timer);
    timer->is_valid = false;
    
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError infrax_timer_reset(struct InfraxCore* self, InfraxU32 timer_id, InfraxU32 interval_ms) {
    if (!g_timers.initialized || timer_id == INVALID_TIMER_ID) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid timer ID or timer system not initialized");
    }
    
    // Find timer
    InfraxTimer* timer = NULL;
    for (size_t i = 0; i < g_timers.capacity; i++) {
        if (g_timers.timers[i].is_valid && g_timers.timers[i].id == timer_id) {
            timer = &g_timers.timers[i];
            break;
        }
    }
    
    if (!timer) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Timer not found");
    }
    
    // Update timer
    timer->interval_ms = interval_ms;
    timer->expire_time = self->time_monotonic_ms(self) + interval_ms;
    
    // Reorder heap
    if (!timer->is_paused) {
        remove_timer_from_heap(timer);
        add_timer_to_heap(timer);
    }
    
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError infrax_timer_pause(struct InfraxCore* self, InfraxU32 timer_id) {
    if (!g_timers.initialized || timer_id == INVALID_TIMER_ID) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid timer ID or timer system not initialized");
    }
    
    // Find timer
    InfraxTimer* timer = NULL;
    for (size_t i = 0; i < g_timers.capacity; i++) {
        if (g_timers.timers[i].is_valid && g_timers.timers[i].id == timer_id) {
            timer = &g_timers.timers[i];
            break;
        }
    }
    
    if (!timer) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Timer not found");
    }
    
    if (!timer->is_paused) {
        timer->is_paused = true;
        remove_timer_from_heap(timer);
    }
    
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError infrax_timer_resume(struct InfraxCore* self, InfraxU32 timer_id) {
    if (!g_timers.initialized || timer_id == INVALID_TIMER_ID) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid timer ID or timer system not initialized");
    }
    
    // Find timer
    InfraxTimer* timer = NULL;
    for (size_t i = 0; i < g_timers.capacity; i++) {
        if (g_timers.timers[i].is_valid && g_timers.timers[i].id == timer_id) {
            timer = &g_timers.timers[i];
            break;
        }
    }
    
    if (!timer) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Timer not found");
    }
    
    if (timer->is_paused) {
        timer->is_paused = false;
        timer->expire_time = self->time_monotonic_ms(self) + timer->interval_ms;
        add_timer_to_heap(timer);
    }
    
    return INFRAX_ERROR_OK_STRUCT;
}

//-----------------------------------------------------------------------------
// Poll Implementation
//-----------------------------------------------------------------------------

static int infrax_poll(struct InfraxCore* self, InfraxPollFd* fds, size_t nfds, int timeout_ms) {
    if (!fds && nfds > 0) {
        return -1;
    }

    // 准备poll结构
    struct pollfd* sys_fds = NULL;
    if (nfds > 0) {
        sys_fds = (struct pollfd*)malloc(nfds * sizeof(struct pollfd));
        if (!sys_fds) {
            return -1;
        }
        
        // 转换为系统pollfd结构
        for (size_t i = 0; i < nfds; i++) {
            sys_fds[i].fd = fds[i].fd;
            sys_fds[i].events = fds[i].events;
            sys_fds[i].revents = 0;
        }
    }

    // 调用系统poll
    int result = poll(sys_fds, nfds, timeout_ms);

    // 如果成功，复制返回的事件
    if (result >= 0 && sys_fds) {
        for (size_t i = 0; i < nfds; i++) {
            fds[i].revents = sys_fds[i].revents;
        }
    }

    // 清理
    if (sys_fds) {
        free(sys_fds);
    }

    return result;
}

// Initialize singleton instance
InfraxCore gInfraxCore = {
    .self = &gInfraxCore,
    .klass = NULL,
    
    // Core functions
    .forward_call = infrax_core_forward_call,
    .printf = infrax_core_printf,
    .snprintf = infrax_core_snprintf,
    .fprintf = infrax_core_fprintf,
    
    // Memory operations
    .malloc = infrax_core_malloc,
    .calloc = infrax_core_calloc,
    .realloc = infrax_core_realloc,
    .free = infrax_core_free,
    
    // String operations
    .strlen = infrax_core_strlen,
    .strcpy = infrax_core_strcpy,
    .strncpy = infrax_core_strncpy,
    .strcat = infrax_core_strcat,
    .strncat = infrax_core_strncat,
    .strcmp = infrax_core_strcmp,
    .strncmp = infrax_core_strncmp,
    .strchr = infrax_core_strchr,
    .strrchr = infrax_core_strrchr,
    .strstr = infrax_core_strstr,
    .strdup = infrax_core_strdup,
    .strndup = infrax_core_strndup,
    
    // Misc operations
    .memcmp = infrax_core_memcmp,
    .memset = infrax_core_memset,
    .memcpy = infrax_core_memcpy,
    .memmove = infrax_core_memmove,
    .isspace = infrax_core_isspace,
    .isdigit = infrax_core_isdigit,
    .hint_yield = infrax_core_hint_yield,
    .pid = infrax_core_pid,
    
    // String to number conversion
    .atoi = infrax_core_atoi,
    .atol = infrax_core_atol,
    .atoll = infrax_core_atoll,
    
    // Random number operations
    .random = infrax_core_random,
    .random_seed = infrax_core_random_seed,
    
    // Network byte order conversion
    .host_to_net16 = infrax_core_host_to_net16,
    .host_to_net32 = infrax_core_host_to_net32,
    .host_to_net64 = infrax_core_host_to_net64,
    .net_to_host16 = infrax_core_net_to_host16,
    .net_to_host32 = infrax_core_net_to_host32,
    .net_to_host64 = infrax_core_net_to_host64,
    
    // Buffer operations
    .buffer_init = infrax_core_buffer_init,
    .buffer_destroy = infrax_core_buffer_destroy,
    .buffer_reserve = infrax_core_buffer_reserve,
    .buffer_write = infrax_core_buffer_write,
    .buffer_read = infrax_core_buffer_read,
    .buffer_readable = infrax_core_buffer_readable,
    .buffer_writable = infrax_core_buffer_writable,
    .buffer_reset = infrax_core_buffer_reset,
    
    // Ring buffer operations
    .ring_buffer_init = infrax_core_ring_buffer_init,
    .ring_buffer_destroy = infrax_core_ring_buffer_destroy,
    .ring_buffer_write = infrax_core_ring_buffer_write,
    .ring_buffer_read = infrax_core_ring_buffer_read,
    .ring_buffer_readable = infrax_core_ring_buffer_readable,
    .ring_buffer_writable = infrax_core_ring_buffer_writable,
    .ring_buffer_reset = infrax_core_ring_buffer_reset,
    
    // File operations
    .file_open = infrax_core_file_open,
    .file_close = infrax_core_file_close,
    .file_read = infrax_core_file_read,
    .file_write = infrax_core_file_write,
    .file_seek = infrax_core_file_seek,
    .file_size = infrax_core_file_size,
    .file_remove = infrax_core_file_remove,
    .file_rename = infrax_core_file_rename,
    .file_exists = infrax_core_file_exists,
    
    // Assert functions
    .assert_failed = infrax_core_assert_failed,
    .set_assert_handler = infrax_core_set_assert_handler,
    
    // File descriptor operations
    .read_fd = infrax_core_read_fd,
    .write_fd = infrax_core_write_fd,
    .create_pipe = infrax_core_create_pipe,
    .set_nonblocking = infrax_core_set_nonblocking,
    .close_fd = infrax_core_close_fd,
    
    // Time operations
    .time_now_ms = infrax_core_time_now_ms,
    .time_monotonic_ms = infrax_core_time_monotonic_ms,
    .clock = infrax_core_clock,
    .clock_gettime = infrax_core_clock_gettime,
    .time = infrax_core_time,
    .clocks_per_sec = infrax_core_clocks_per_sec,
    .sleep = infrax_core_sleep,
    .sleep_ms = infrax_core_sleep_ms,
    .sleep_us = infrax_core_sleep_us,
    .get_memory_usage = infrax_core_get_memory_usage,
    
    // Signal operations
    .signal = infrax_core_signal,
    .alarm = infrax_core_alarm,

    // Network operations
    .socket_create = infrax_core_socket_create,
    .socket_bind = infrax_core_socket_bind,
    .socket_listen = infrax_core_socket_listen,
    .socket_accept = infrax_core_socket_accept,
    .socket_connect = infrax_core_socket_connect,
    .socket_send = infrax_core_socket_send,
    .socket_recv = infrax_core_socket_recv,
    .socket_close = infrax_core_socket_close,
    .socket_shutdown = infrax_core_socket_shutdown,
    .socket_set_option = infrax_core_socket_set_option,
    .socket_get_option = infrax_core_socket_get_option,
    .socket_get_error = infrax_core_socket_get_error,
    .socket_get_name = infrax_core_socket_get_name,
    .socket_get_peer = infrax_core_socket_get_peer,

    // Network address operations
    .ip_to_binary = infrax_core_ip_to_binary,
    .binary_to_ip = infrax_core_binary_to_ip,

    // Error handling
    .get_last_error = infrax_core_get_last_error,
    .get_error_string = infrax_core_get_error_string,

    // File descriptor set operations
    .fd_zero = infrax_core_fd_zero,
    .fd_set = infrax_core_fd_set,
    .fd_clr = infrax_core_fd_clr,
    .fd_isset = infrax_core_fd_isset,
    
    // File descriptor control operations
    .fcntl = infrax_core_fcntl,
    
    // IO multiplexing
    .select = infrax_core_select,
    
    // High-level IO operations
    .wait_for_read = infrax_core_wait_for_read,
    .wait_for_write = infrax_core_wait_for_write,
    .wait_for_except = infrax_core_wait_for_except,

    // Poll operations
    .poll = infrax_poll,
    
    // Timer operations
    .timer_create = infrax_timer_create,
    .timer_delete = infrax_timer_delete,
    .timer_reset = infrax_timer_reset,
    .timer_pause = infrax_timer_pause,
    .timer_resume = infrax_timer_resume,
};

// Simple singleton getter
InfraxCore* infrax_core_singleton(void) {
    return &gInfraxCore;
};

InfraxCoreClassType InfraxCoreClass = {
    .singleton = infrax_core_singleton
};
