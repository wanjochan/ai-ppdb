#include "cosmopolitan.h"
#include "internal/infra/infra.h"

/* Statistics */
static struct {
    u64 alloc_count;
    u64 free_count;
    u64 total_allocated;
    u64 current_allocated;
    u64 error_count;
} g_stats = {0};

/* Debug Logging */
static int g_debug_level = INFRA_LOG_INFO;
static void (*g_log_handler)(int level, const char* msg) = NULL;

void infra_set_log_level(int level) {
    g_debug_level = level;
}

void infra_set_log_handler(void (*handler)(int level, const char* msg)) {
    g_log_handler = handler;
}

void infra_log(int level, const char* fmt, ...) {
    if (level < g_debug_level) return;
    
    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
    
    if (g_log_handler) {
        g_log_handler(level, msg);
    } else {
        fprintf(stderr, "[%d] %s\n", level, msg);
    }
}

/* Error Handling */
static const char* error_strings[] = {
    "Success",
    "Out of memory",
    "Invalid argument",
    "Not found",
    "Already exists",
    "Busy",
    "Timeout",
    "Network error",
    "Connection refused",
    "Connection timeout",
    "Connection closed"
};

static int g_error_code = 0;
static char g_error_msg[256] = {0};

void infra_set_debug_level(int level) {
    g_debug_level = level;
}

const char* infra_strerror(int code) {
    if (code < 0 || code >= sizeof(error_strings)/sizeof(error_strings[0])) {
        return "Unknown error";
    }
    return error_strings[code];
}

void infra_set_error(int code, const char* msg) {
    g_error_code = code;
    if (msg) {
        strncpy(g_error_msg, msg, sizeof(g_error_msg) - 1);
        g_error_msg[sizeof(g_error_msg) - 1] = '\0';
    } else {
        g_error_msg[0] = '\0';
    }
}

const char* infra_get_error(void) {
    if (g_error_msg[0]) {
        return g_error_msg;
    }
    return infra_strerror(g_error_code);
}

/* Memory Management */
void* infra_malloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr) {
        g_stats.alloc_count++;
        g_stats.total_allocated += size;
        g_stats.current_allocated += size;
        infra_log(INFRA_LOG_DEBUG, "malloc(%zu) = %p", size, ptr);
    } else {
        g_stats.error_count++;
        infra_log(INFRA_LOG_ERROR, "malloc(%zu) failed", size);
    }
    return ptr;
}

void* infra_calloc(size_t nmemb, size_t size) {
    void* ptr = calloc(nmemb, size);
    if (ptr) {
        g_stats.alloc_count++;
        g_stats.total_allocated += (nmemb * size);
        g_stats.current_allocated += (nmemb * size);
        infra_log(INFRA_LOG_DEBUG, "calloc(%zu, %zu) = %p", nmemb, size, ptr);
    } else {
        g_stats.error_count++;
        infra_log(INFRA_LOG_ERROR, "calloc(%zu, %zu) failed", nmemb, size);
    }
    return ptr;
}

void* infra_realloc(void* old_ptr, size_t new_size) {
    void* ptr = realloc(old_ptr, new_size);
    if (ptr) {
        if (!old_ptr) {
            g_stats.alloc_count++;
        }
        g_stats.total_allocated += new_size;
        infra_log(INFRA_LOG_DEBUG, "realloc(%p, %zu) = %p", old_ptr, new_size, ptr);
    } else {
        g_stats.error_count++;
        infra_log(INFRA_LOG_ERROR, "realloc(%p, %zu) failed", old_ptr, new_size);
    }
    return ptr;
}

void infra_free(void* ptr) {
    if (ptr) {
        g_stats.free_count++;
        infra_log(INFRA_LOG_DEBUG, "free(%p)", ptr);
        free(ptr);
    }
}

/* Statistics Interface */
void infra_get_stats(struct infra_stats* stats) {
    if (stats) {
        stats->alloc_count = g_stats.alloc_count;
        stats->free_count = g_stats.free_count;
        stats->total_allocated = g_stats.total_allocated;
        stats->current_allocated = g_stats.current_allocated;
        stats->error_count = g_stats.error_count;
    }
}

void infra_reset_stats(void) {
    memset(&g_stats, 0, sizeof(g_stats));
    infra_log(INFRA_LOG_INFO, "Statistics reset");
}
