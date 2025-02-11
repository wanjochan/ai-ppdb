#include "internal/infrax/InfraxCore.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/resource.h>

int infrax_core_printf(InfraxCore *self, const char* format, ...) {
    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
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
    rb->full = false;
    return INFRAX_ERROR_OK_STRUCT;
}

static void infrax_core_ring_buffer_destroy(InfraxCore *self, InfraxRingBuffer* rb) {
    if (rb && rb->buffer) {
        free(rb->buffer);
        rb->buffer = NULL;
        rb->size = 0;
        rb->read_pos = 0;
        rb->write_pos = 0;
        rb->full = false;
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
    rb->full = false;
    return INFRAX_ERROR_OK_STRUCT;
}

static void infrax_core_ring_buffer_reset(InfraxCore *self, InfraxRingBuffer* rb) {
    if (rb) {
        rb->read_pos = 0;
        rb->write_pos = 0;
        rb->full = false;
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

static InfraxError infrax_core_file_exists(InfraxCore *self, const char* path, bool* exists) {
    if (!path || !exists) {
        return make_error(INFRAX_ERROR_INVALID_PARAM, "Invalid parameters");
    }
    
    struct stat st;
    *exists = (stat(path, &st) == 0);
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

static time_t infrax_core_time(InfraxCore *self, time_t* tloc) {
    return time(tloc);
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


// Initialize singleton instance
static InfraxCore singleton = {
    .self = &singleton,  // Self pointer to the static instance
    
    // Core functions
    .forward_call = infrax_core_forward_call,
    .printf = infrax_core_printf,
    .snprintf = infrax_core_snprintf,
    
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
    
    // Time management
    .time_now_ms = infrax_core_time_now_ms,
    .time_monotonic_ms = infrax_core_time_monotonic_ms,
    .sleep_ms = infrax_core_sleep_ms,
    .hint_yield = infrax_core_hint_yield,
    .pid = infrax_core_pid,
    
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
    .ring_buffer_readable = infrax_core_ring_buffer_readable,
    .ring_buffer_writable = infrax_core_ring_buffer_writable,
    .ring_buffer_init = infrax_core_ring_buffer_init,
    .ring_buffer_destroy = infrax_core_ring_buffer_destroy,
    .ring_buffer_write = infrax_core_ring_buffer_write,
    .ring_buffer_read = infrax_core_ring_buffer_read,
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
    .assert_failed = infrax_core_assert_failed,
    .set_assert_handler = infrax_core_set_assert_handler,
    
    // File descriptor operations
    .read_fd = infrax_core_read_fd,
    .write_fd = infrax_core_write_fd,
    .create_pipe = infrax_core_create_pipe,
    .set_nonblocking = infrax_core_set_nonblocking,
    .close_fd = infrax_core_close_fd,
    
    // Time operations
    .clock = infrax_core_clock,
    .clock_gettime = infrax_core_clock_gettime,
    .time = infrax_core_time,
    .clocks_per_sec = infrax_core_clocks_per_sec,
    .sleep = infrax_core_sleep,
    .sleep_us = infrax_core_sleep_us,
    .get_memory_usage = infrax_core_get_memory_usage,
    
};

// Simple singleton getter
InfraxCore* infrax_core_singleton(void) {
    return &singleton;
};

const InfraxCoreClassType InfraxCoreClass = {
    .singleton = infrax_core_singleton
};
