#include "cosmopolitan.h"
#include "internal/infra/infra.h"

/* Error strings */
static const char* error_strings[] = {
    "Success",
    "Invalid parameter",
    "Out of memory",
    "Thread error",
    "Mutex error",
    "Condition variable error",
    "Read-write lock error",
    "Resource busy",
    "Not found",
    "Already exists",
    "IO error",
    "Timeout",
    "Operation cancelled"
};

/* Memory Management */
ppdb_error_t ppdb_mem_malloc(size_t size, void** ptr) {
    if (!ptr || size == 0) {
        return PPDB_ERR_PARAM;
    }

    void* mem = malloc(size);
    if (!mem) {
        return PPDB_ERR_MEMORY;
    }

    *ptr = mem;
    return PPDB_OK;
}

ppdb_error_t ppdb_mem_calloc(size_t nmemb, size_t size, void** ptr) {
    if (!ptr || nmemb == 0 || size == 0) {
        return PPDB_ERR_PARAM;
    }

    void* mem = calloc(nmemb, size);
    if (!mem) {
        return PPDB_ERR_MEMORY;
    }

    *ptr = mem;
    return PPDB_OK;
}

ppdb_error_t ppdb_mem_realloc(void* old_ptr, size_t size, void** new_ptr) {
    if (!new_ptr || size == 0) {
        return PPDB_ERR_PARAM;
    }

    void* mem = realloc(old_ptr, size);
    if (!mem) {
        return PPDB_ERR_MEMORY;
    }

    *new_ptr = mem;
    return PPDB_OK;
}

void ppdb_mem_free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

/* Log Level */
static int g_log_level = INFRA_LOG_INFO;
static void (*g_log_handler)(int level, const char* msg) = NULL;

/* Core Functions */
void infra_set_log_level(int level) {
    g_log_level = level;
}

void infra_set_log_handler(void (*handler)(int level, const char* msg)) {
    g_log_handler = handler;
}

void infra_log(int level, const char* fmt, ...) {
    if (level > g_log_level) {
        return;
    }

    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    if (g_log_handler) {
        g_log_handler(level, msg);
    } else {
        fprintf(stderr, "%s\n", msg);
    }
}

const char* infra_strerror(int code) {
    if (code < 0 || code >= sizeof(error_strings)/sizeof(error_strings[0])) {
        return "Unknown error";
    }
    return error_strings[code];
}

/* Error Handling */
static char g_error_msg[256];

void infra_set_error(int code, const char* msg) {
    if (msg) {
        snprintf(g_error_msg, sizeof(g_error_msg), "%s: %s", infra_strerror(code), msg);
    } else {
        snprintf(g_error_msg, sizeof(g_error_msg), "%s", infra_strerror(code));
    }
}

const char* infra_get_error(void) {
    return g_error_msg;
}

/* Statistics */
static struct infra_stats g_stats = {0};

void infra_get_stats(struct infra_stats* stats) {
    if (stats) {
        *stats = g_stats;
    }
}

void infra_reset_stats(void) {
    memset(&g_stats, 0, sizeof(g_stats));
}
