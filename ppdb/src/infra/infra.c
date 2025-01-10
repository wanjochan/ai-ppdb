/*
 * infra.c - Infrastructure Layer Implementation
 */

#include "internal/infra/infra.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

//-----------------------------------------------------------------------------
// Error Handling
//-----------------------------------------------------------------------------

static const char* error_strings[] = {
    [INFRA_OK]                = "Success",
    [INFRA_ERROR_GENERIC]     = "Generic error",
    [INFRA_ERROR_MEMORY]      = "Memory error",
    [INFRA_ERROR_IO]          = "I/O error",
    [INFRA_ERROR_TIMEOUT]     = "Timeout error",
    [INFRA_ERROR_BUSY]        = "Resource busy",
    [INFRA_ERROR_AGAIN]       = "Try again",
    [INFRA_ERROR_INVALID]     = "Invalid argument",
    [INFRA_ERROR_NOTFOUND]    = "Not found",
    [INFRA_ERROR_EXISTS]      = "Already exists",
    [INFRA_ERROR_FULL]        = "Resource full",
    [INFRA_ERROR_EMPTY]       = "Resource empty",
    [INFRA_ERROR_OVERFLOW]    = "Overflow error",
    [INFRA_ERROR_UNDERFLOW]   = "Underflow error",
    [INFRA_ERROR_SYSTEM]      = "System error",
    [INFRA_ERROR_PROTOCOL]    = "Protocol error",
    [INFRA_ERROR_NETWORK]     = "Network error",
    [INFRA_ERROR_SECURITY]    = "Security error"
};

const char* infra_error_string(infra_error_t error) {
    if (error >= 0) {
        return error_strings[0];  // Success
    }
    error = -error;  // Convert to positive index
    if (error >= sizeof(error_strings) / sizeof(error_strings[0])) {
        return "Unknown error";
    }
    return error_strings[error];
}

//-----------------------------------------------------------------------------
// Memory Management
//-----------------------------------------------------------------------------

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

void* infra_memset(void* s, int c, size_t n) {
    return memset(s, c, n);
}

void* infra_memcpy(void* dest, const void* src, size_t n) {
    return memcpy(dest, src, n);
}

void* infra_memmove(void* dest, const void* src, size_t n) {
    return memmove(dest, src, n);
}

int infra_memcmp(const void* s1, const void* s2, size_t n) {
    return memcmp(s1, s2, n);
}

//-----------------------------------------------------------------------------
// String Operations
//-----------------------------------------------------------------------------

size_t infra_strlen(const char* s) {
    return strlen(s);
}

char* infra_strcpy(char* dest, const char* src) {
    return strcpy(dest, src);
}

char* infra_strncpy(char* dest, const char* src, size_t n) {
    return strncpy(dest, src, n);
}

char* infra_strcat(char* dest, const char* src) {
    return strcat(dest, src);
}

char* infra_strncat(char* dest, const char* src, size_t n) {
    return strncat(dest, src, n);
}

int infra_strcmp(const char* s1, const char* s2) {
    return strcmp(s1, s2);
}

int infra_strncmp(const char* s1, const char* s2, size_t n) {
    return strncmp(s1, s2, n);
}

char* infra_strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* new_str = infra_malloc(len);
    if (new_str) {
        memcpy(new_str, s, len);
    }
    return new_str;
}

char* infra_strndup(const char* s, size_t n) {
    size_t len = strnlen(s, n);
    char* new_str = infra_malloc(len + 1);
    if (new_str) {
        memcpy(new_str, s, len);
        new_str[len] = '\0';
    }
    return new_str;
}

char* infra_strchr(const char* s, int c) {
    return strchr(s, c);
}

char* infra_strrchr(const char* s, int c) {
    return strrchr(s, c);
}

char* infra_strstr(const char* haystack, const char* needle) {
    return strstr(haystack, needle);
}

//-----------------------------------------------------------------------------
// Buffer Operations
//-----------------------------------------------------------------------------

infra_error_t infra_buffer_init(infra_buffer_t* buf, size_t initial_capacity) {
    if (!buf || initial_capacity == 0) {
        return INFRA_ERROR_INVALID;
    }

    buf->data = infra_malloc(initial_capacity);
    if (!buf->data) {
        return INFRA_ERROR_MEMORY;
    }

    buf->size = 0;
    buf->capacity = initial_capacity;
    return INFRA_OK;
}

void infra_buffer_destroy(infra_buffer_t* buf) {
    if (buf) {
        infra_free(buf->data);
        buf->data = NULL;
        buf->size = 0;
        buf->capacity = 0;
    }
}

infra_error_t infra_buffer_reserve(infra_buffer_t* buf, size_t capacity) {
    if (!buf) {
        return INFRA_ERROR_INVALID;
    }

    if (capacity <= buf->capacity) {
        return INFRA_OK;
    }

    uint8_t* new_data = infra_realloc(buf->data, capacity);
    if (!new_data) {
        return INFRA_ERROR_MEMORY;
    }

    buf->data = new_data;
    buf->capacity = capacity;
    return INFRA_OK;
}

infra_error_t infra_buffer_write(infra_buffer_t* buf, const void* data, size_t size) {
    if (!buf || !data || size == 0) {
        return INFRA_ERROR_INVALID;
    }

    if (buf->size + size > buf->capacity) {
        size_t new_capacity = buf->capacity * 2;
        if (new_capacity < buf->size + size) {
            new_capacity = buf->size + size;
        }
        infra_error_t err = infra_buffer_reserve(buf, new_capacity);
        if (err != INFRA_OK) {
            return err;
        }
    }

    memcpy(buf->data + buf->size, data, size);
    buf->size += size;
    return INFRA_OK;
}

infra_error_t infra_buffer_read(infra_buffer_t* buf, void* data, size_t size) {
    if (!buf || !data || size == 0) {
        return INFRA_ERROR_INVALID;
    }

    if (size > buf->size) {
        return INFRA_ERROR_OVERFLOW;
    }

    memcpy(data, buf->data, size);
    memmove(buf->data, buf->data + size, buf->size - size);
    buf->size -= size;
    return INFRA_OK;
}

size_t infra_buffer_readable(const infra_buffer_t* buf) {
    return buf ? buf->size : 0;
}

size_t infra_buffer_writable(const infra_buffer_t* buf) {
    return buf ? buf->capacity - buf->size : 0;
}

void infra_buffer_reset(infra_buffer_t* buf) {
    if (buf) {
        buf->size = 0;
    }
}

//-----------------------------------------------------------------------------
// Logging
//-----------------------------------------------------------------------------

static int current_log_level = INFRA_LOG_LEVEL_INFO;
static infra_log_callback_t log_callback = NULL;

void infra_log_set_level(int level) {
    if (level >= INFRA_LOG_LEVEL_NONE && level <= INFRA_LOG_LEVEL_TRACE) {
        current_log_level = level;
    }
}

void infra_log_set_callback(infra_log_callback_t callback) {
    log_callback = callback;
}

void infra_log(int level, const char* file, int line, const char* func,
               const char* format, ...) {
    if (level > current_log_level) {
        return;
    }

    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (log_callback) {
        log_callback(level, file, line, func, message);
    } else {
        const char* level_str = "UNKNOWN";
        switch (level) {
            case INFRA_LOG_LEVEL_ERROR: level_str = "ERROR"; break;
            case INFRA_LOG_LEVEL_WARN:  level_str = "WARN"; break;
            case INFRA_LOG_LEVEL_INFO:  level_str = "INFO"; break;
            case INFRA_LOG_LEVEL_DEBUG: level_str = "DEBUG"; break;
            case INFRA_LOG_LEVEL_TRACE: level_str = "TRACE"; break;
        }
        fprintf(stderr, "[%s] %s:%d %s(): %s\n", level_str, file, line, func, message);
    }
}

//-----------------------------------------------------------------------------
// Statistics
//-----------------------------------------------------------------------------

void infra_stats_init(infra_stats_t* stats) {
    if (stats) {
        memset(stats, 0, sizeof(infra_stats_t));
        stats->min_latency_us = UINT64_MAX;
    }
}

void infra_stats_reset(infra_stats_t* stats) {
    infra_stats_init(stats);
}

void infra_stats_update(infra_stats_t* stats, bool success, uint64_t latency_us,
                       size_t bytes, infra_error_t error) {
    if (!stats) {
        return;
    }

    stats->total_operations++;
    if (success) {
        stats->successful_operations++;
    } else {
        stats->failed_operations++;
        stats->last_error = error;
        stats->last_error_time = infra_time_monotonic_ms();
    }

    stats->total_bytes += bytes;

    if (latency_us < stats->min_latency_us) {
        stats->min_latency_us = latency_us;
    }
    if (latency_us > stats->max_latency_us) {
        stats->max_latency_us = latency_us;
    }

    // Update average latency using exponential moving average
    if (stats->avg_latency_us == 0) {
        stats->avg_latency_us = latency_us;
    } else {
        stats->avg_latency_us = (stats->avg_latency_us * 7 + latency_us) / 8;
    }
}

void infra_stats_merge(infra_stats_t* dest, const infra_stats_t* src) {
    if (!dest || !src) {
        return;
    }

    dest->total_operations += src->total_operations;
    dest->successful_operations += src->successful_operations;
    dest->failed_operations += src->failed_operations;
    dest->total_bytes += src->total_bytes;

    if (src->min_latency_us < dest->min_latency_us) {
        dest->min_latency_us = src->min_latency_us;
    }
    if (src->max_latency_us > dest->max_latency_us) {
        dest->max_latency_us = src->max_latency_us;
    }

    // Merge average latencies weighted by operation counts
    uint64_t total_ops = dest->total_operations + src->total_operations;
    if (total_ops > 0) {
        dest->avg_latency_us = (dest->avg_latency_us * dest->total_operations +
                               src->avg_latency_us * src->total_operations) / total_ops;
    }

    // Keep the most recent error
    if (src->last_error_time > dest->last_error_time) {
        dest->last_error = src->last_error;
        dest->last_error_time = src->last_error_time;
    }
}

void infra_stats_print(const infra_stats_t* stats, const char* prefix) {
    if (!stats || !prefix) {
        return;
    }

    printf("%s Statistics:\n", prefix);
    printf("  Total Operations: %lu\n", stats->total_operations);
    printf("  Successful Operations: %lu\n", stats->successful_operations);
    printf("  Failed Operations: %lu\n", stats->failed_operations);
    printf("  Total Bytes: %lu\n", stats->total_bytes);
    printf("  Min Latency: %lu us\n", stats->min_latency_us);
    printf("  Max Latency: %lu us\n", stats->max_latency_us);
    printf("  Avg Latency: %lu us\n", stats->avg_latency_us);
    if (stats->last_error) {
        printf("  Last Error: %s (at %lu ms)\n",
               infra_error_string(stats->last_error),
               stats->last_error_time);
    }
}

//-----------------------------------------------------------------------------
// Data Structures - List
//-----------------------------------------------------------------------------

typedef struct infra_list_node {
    struct infra_list_node* prev;
    struct infra_list_node* next;
    void* data;
} infra_list_node_t;

typedef struct infra_list {
    infra_list_node_t* head;
    infra_list_node_t* tail;
    size_t size;
} infra_list_t;

infra_error_t infra_list_init(infra_list_t* list) {
    if (!list) {
        return INFRA_ERROR_INVALID;
    }
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    return INFRA_OK;
}

void infra_list_destroy(infra_list_t* list) {
    if (!list) {
        return;
    }
    infra_list_node_t* node = list->head;
    while (node) {
        infra_list_node_t* next = node->next;
        infra_free(node);
        node = next;
    }
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

infra_error_t infra_list_push_back(infra_list_t* list, void* data) {
    if (!list) {
        return INFRA_ERROR_INVALID;
    }

    infra_list_node_t* node = infra_malloc(sizeof(infra_list_node_t));
    if (!node) {
        return INFRA_ERROR_MEMORY;
    }

    node->data = data;
    node->next = NULL;
    node->prev = list->tail;

    if (list->tail) {
        list->tail->next = node;
    } else {
        list->head = node;
    }
    list->tail = node;
    list->size++;

    return INFRA_OK;
}

infra_error_t infra_list_push_front(infra_list_t* list, void* data) {
    if (!list) {
        return INFRA_ERROR_INVALID;
    }

    infra_list_node_t* node = infra_malloc(sizeof(infra_list_node_t));
    if (!node) {
        return INFRA_ERROR_MEMORY;
    }

    node->data = data;
    node->prev = NULL;
    node->next = list->head;

    if (list->head) {
        list->head->prev = node;
    } else {
        list->tail = node;
    }
    list->head = node;
    list->size++;

    return INFRA_OK;
}

void* infra_list_pop_back(infra_list_t* list) {
    if (!list || !list->tail) {
        return NULL;
    }

    infra_list_node_t* node = list->tail;
    void* data = node->data;

    list->tail = node->prev;
    if (list->tail) {
        list->tail->next = NULL;
    } else {
        list->head = NULL;
    }

    infra_free(node);
    list->size--;

    return data;
}

void* infra_list_pop_front(infra_list_t* list) {
    if (!list || !list->head) {
        return NULL;
    }

    infra_list_node_t* node = list->head;
    void* data = node->data;

    list->head = node->next;
    if (list->head) {
        list->head->prev = NULL;
    } else {
        list->tail = NULL;
    }

    infra_free(node);
    list->size--;

    return data;
}

size_t infra_list_size(const infra_list_t* list) {
    return list ? list->size : 0;
}

bool infra_list_empty(const infra_list_t* list) {
    return list ? list->size == 0 : true;
}

//-----------------------------------------------------------------------------
// Data Structures - Hash Table
//-----------------------------------------------------------------------------

#define INFRA_HASH_INITIAL_SIZE 16
#define INFRA_HASH_LOAD_FACTOR 0.75

typedef struct infra_hash_entry {
    char* key;
    void* value;
    struct infra_hash_entry* next;
} infra_hash_entry_t;

typedef struct infra_hash_table {
    infra_hash_entry_t** buckets;
    size_t size;
    size_t capacity;
} infra_hash_table_t;

static size_t infra_hash_function(const char* key) {
    size_t hash = 5381;
    int c;
    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

infra_error_t infra_hash_init(infra_hash_table_t* table) {
    if (!table) {
        return INFRA_ERROR_INVALID;
    }

    table->buckets = infra_calloc(INFRA_HASH_INITIAL_SIZE, sizeof(infra_hash_entry_t*));
    if (!table->buckets) {
        return INFRA_ERROR_MEMORY;
    }

    table->size = 0;
    table->capacity = INFRA_HASH_INITIAL_SIZE;
    return INFRA_OK;
}

void infra_hash_destroy(infra_hash_table_t* table) {
    if (!table) {
        return;
    }

    for (size_t i = 0; i < table->capacity; i++) {
        infra_hash_entry_t* entry = table->buckets[i];
        while (entry) {
            infra_hash_entry_t* next = entry->next;
            infra_free(entry->key);
            infra_free(entry);
            entry = next;
        }
    }

    infra_free(table->buckets);
    table->buckets = NULL;
    table->size = 0;
    table->capacity = 0;
}

static infra_error_t infra_hash_resize(infra_hash_table_t* table) {
    size_t new_capacity = table->capacity * 2;
    infra_hash_entry_t** new_buckets = infra_calloc(new_capacity, sizeof(infra_hash_entry_t*));
    if (!new_buckets) {
        return INFRA_ERROR_MEMORY;
    }

    for (size_t i = 0; i < table->capacity; i++) {
        infra_hash_entry_t* entry = table->buckets[i];
        while (entry) {
            infra_hash_entry_t* next = entry->next;
            size_t index = infra_hash_function(entry->key) % new_capacity;
            entry->next = new_buckets[index];
            new_buckets[index] = entry;
            entry = next;
        }
    }

    infra_free(table->buckets);
    table->buckets = new_buckets;
    table->capacity = new_capacity;
    return INFRA_OK;
}

infra_error_t infra_hash_put(infra_hash_table_t* table, const char* key, void* value) {
    if (!table || !key) {
        return INFRA_ERROR_INVALID;
    }

    if ((float)table->size / table->capacity > INFRA_HASH_LOAD_FACTOR) {
        infra_error_t err = infra_hash_resize(table);
        if (err != INFRA_OK) {
            return err;
        }
    }

    size_t index = infra_hash_function(key) % table->capacity;
    infra_hash_entry_t* entry = table->buckets[index];

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            return INFRA_OK;
        }
        entry = entry->next;
    }

    entry = infra_malloc(sizeof(infra_hash_entry_t));
    if (!entry) {
        return INFRA_ERROR_MEMORY;
    }

    entry->key = infra_strdup(key);
    if (!entry->key) {
        infra_free(entry);
        return INFRA_ERROR_MEMORY;
    }

    entry->value = value;
    entry->next = table->buckets[index];
    table->buckets[index] = entry;
    table->size++;

    return INFRA_OK;
}

void* infra_hash_get(const infra_hash_table_t* table, const char* key) {
    if (!table || !key) {
        return NULL;
    }

    size_t index = infra_hash_function(key) % table->capacity;
    infra_hash_entry_t* entry = table->buckets[index];

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }

    return NULL;
}

bool infra_hash_remove(infra_hash_table_t* table, const char* key) {
    if (!table || !key) {
        return false;
    }

    size_t index = infra_hash_function(key) % table->capacity;
    infra_hash_entry_t* entry = table->buckets[index];
    infra_hash_entry_t* prev = NULL;

    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            if (prev) {
                prev->next = entry->next;
            } else {
                table->buckets[index] = entry->next;
            }
            infra_free(entry->key);
            infra_free(entry);
            table->size--;
            return true;
        }
        prev = entry;
        entry = entry->next;
    }

    return false;
}

size_t infra_hash_size(const infra_hash_table_t* table) {
    return table ? table->size : 0;
}

bool infra_hash_empty(const infra_hash_table_t* table) {
    return table ? table->size == 0 : true;
}

//-----------------------------------------------------------------------------
// Data Structures - Queue
//-----------------------------------------------------------------------------

typedef struct infra_queue {
    infra_list_t list;
} infra_queue_t;

infra_error_t infra_queue_init(infra_queue_t* queue) {
    if (!queue) {
        return INFRA_ERROR_INVALID;
    }
    return infra_list_init(&queue->list);
}

void infra_queue_destroy(infra_queue_t* queue) {
    if (queue) {
        infra_list_destroy(&queue->list);
    }
}

infra_error_t infra_queue_push(infra_queue_t* queue, void* data) {
    if (!queue) {
        return INFRA_ERROR_INVALID;
    }
    return infra_list_push_back(&queue->list, data);
}

void* infra_queue_pop(infra_queue_t* queue) {
    if (!queue) {
        return NULL;
    }
    return infra_list_pop_front(&queue->list);
}

void* infra_queue_peek(const infra_queue_t* queue) {
    if (!queue || !queue->list.head) {
        return NULL;
    }
    return queue->list.head->data;
}

size_t infra_queue_size(const infra_queue_t* queue) {
    return queue ? queue->list.size : 0;
}

bool infra_queue_empty(const infra_queue_t* queue) {
    return queue ? queue->list.size == 0 : true;
}

