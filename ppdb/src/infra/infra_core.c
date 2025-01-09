#include "cosmopolitan.h"
#include "internal/infra/infra.h"

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
    return malloc(size);
}

void* infra_calloc(size_t nmemb, size_t size) {
    return calloc(nmemb, size);
}

void* infra_realloc(void* ptr, size_t size) {
    return realloc(ptr, size);
}

void infra_free(void* ptr) {
    free(ptr);
}
