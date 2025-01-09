/*
 * base_core.inc.c - Core Infrastructure Implementation
 *
 * This file contains:
 * 1. Memory management and error handling
 * 2. String operations and configuration
 * 3. File system operations
 * 4. Logging system
 * 5. Time and system utilities
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Global variables
static ppdb_error_context_t g_error_context = {0};
static ppdb_base_mutex_t* g_error_mutex = NULL;
static ppdb_error_stats_t g_error_stats = {0};

//-----------------------------------------------------------------------------
// Memory Management
//-----------------------------------------------------------------------------
ppdb_error_t ppdb_base_mem_malloc(size_t size, void** out_ptr) {
    if (!out_ptr) return PPDB_ERR_PARAM;
    
    void* ptr = malloc(size);
    if (!ptr) {
        *out_ptr = NULL;
        return PPDB_ERR_MEMORY;
    }
    
    *out_ptr = ptr;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_mem_calloc(size_t count, size_t size, void** out_ptr) {
    if (!out_ptr) return PPDB_ERR_PARAM;
    
    void* ptr = calloc(count, size);
    if (!ptr) {
        *out_ptr = NULL;
        return PPDB_ERR_MEMORY;
    }
    
    *out_ptr = ptr;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_mem_realloc(void* ptr, size_t new_size, void** out_ptr) {
    if (!out_ptr) return PPDB_ERR_PARAM;
    
    void* new_ptr = realloc(ptr, new_size);
    if (!new_ptr) {
        *out_ptr = NULL;
        return PPDB_ERR_MEMORY;
    }
    
    *out_ptr = new_ptr;
    return PPDB_OK;
}

void ppdb_base_mem_free(void* ptr) {
    if (ptr) {
        free(ptr);
    }
}

//-----------------------------------------------------------------------------
// Error Handling
//-----------------------------------------------------------------------------

// Initialize error handling
ppdb_error_t ppdb_base_error_init(void) {
    if (g_error_mutex) return PPDB_OK;
    
    ppdb_error_t err = ppdb_base_mutex_create(&g_error_mutex);
    if (err != PPDB_OK) return err;
    
    memset(&g_error_context, 0, sizeof(ppdb_error_context_t));
    g_error_context.max_frames = PPDB_ERROR_MAX_FRAMES;
    ppdb_base_error_stats_reset();
    
    return PPDB_OK;
}

// Cleanup error handling
void ppdb_base_error_cleanup(void) {
    if (!g_error_mutex) return;
    
    ppdb_base_mutex_lock(g_error_mutex);
    
    // Free error frames
    ppdb_error_frame_t* frame = g_error_context.stack;
    while (frame) {
        ppdb_error_frame_t* next = frame->next;
        ppdb_base_mem_free(frame);
        frame = next;
    }
    
    // Reset context
    ppdb_error_stats_t stats = g_error_context.stats;
    memset(&g_error_context, 0, sizeof(ppdb_error_context_t));
    g_error_context.stats = stats;
    
    ppdb_base_mutex_unlock(g_error_mutex);
    
    ppdb_base_mutex_destroy(g_error_mutex);
    g_error_mutex = NULL;
}

// Set error with details
ppdb_error_t ppdb_base_error_set(ppdb_error_t code,
                               ppdb_error_severity_t severity,
                               ppdb_error_category_t category,
                               const char* file,
                               int line,
                               const char* func,
                               const char* message) {
    if (!g_error_mutex) return PPDB_ERR_NOT_INITIALIZED;
    
    ppdb_base_mutex_lock(g_error_mutex);
    
    // Update error context
    g_error_context.error = code;
    g_error_context.severity = severity;
    g_error_context.category = category;
    g_error_context.file = file;
    g_error_context.line = line;
    g_error_context.func = func;
    
    if (message) {
        strncpy(g_error_context.message, message, sizeof(g_error_context.message) - 1);
        g_error_context.message[sizeof(g_error_context.message) - 1] = '\0';
    } else {
        g_error_context.message[0] = '\0';
    }
    
    // Update statistics
    g_error_context.stats.total_errors++;
    g_error_context.stats.errors_by_category[category]++;
    g_error_context.stats.last_error_time = time(NULL);
    
    switch (severity) {
        case PPDB_ERROR_SEVERITY_INFO:
            g_error_context.stats.info_count++;
            break;
        case PPDB_ERROR_SEVERITY_WARNING:
            g_error_context.stats.warning_count++;
            break;
        case PPDB_ERROR_SEVERITY_ERROR:
            g_error_context.stats.error_count++;
            break;
        case PPDB_ERROR_SEVERITY_FATAL:
            g_error_context.stats.fatal_count++;
            break;
    }
    
    // Call error callback if set
    if (g_error_context.callback) {
        g_error_context.callback(code, &g_error_context, g_error_context.callback_data);
    }
    
    ppdb_base_mutex_unlock(g_error_mutex);
    return code;
}

// Push error frame to stack
ppdb_error_t ppdb_base_error_push_frame(const char* file,
                                      int line,
                                      const char* func,
                                      const char* message) {
    if (!g_error_mutex) return PPDB_ERR_NOT_INITIALIZED;
    
    ppdb_base_mutex_lock(g_error_mutex);
    
    // Check stack overflow
    if (g_error_context.frame_count >= g_error_context.max_frames) {
        ppdb_base_mutex_unlock(g_error_mutex);
        return PPDB_ERR_STACK_OVERFLOW;
    }
    
    // Allocate new frame
    ppdb_error_frame_t* frame = NULL;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_error_frame_t), (void**)&frame);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(g_error_mutex);
        return err;
    }
    
    // Initialize frame
    frame->file = file;
    frame->line = line;
    frame->func = func;
    if (message) {
        strncpy(frame->message, message, sizeof(frame->message) - 1);
        frame->message[sizeof(frame->message) - 1] = '\0';
    } else {
        frame->message[0] = '\0';
    }
    
    // Add to stack
    frame->next = g_error_context.stack;
    g_error_context.stack = frame;
    g_error_context.frame_count++;
    
    ppdb_base_mutex_unlock(g_error_mutex);
    return PPDB_OK;
}

// Pop error frame from stack
void ppdb_base_error_pop_frame(void) {
    if (!g_error_mutex) return;
    
    ppdb_base_mutex_lock(g_error_mutex);
    
    if (g_error_context.stack) {
        ppdb_error_frame_t* frame = g_error_context.stack;
        g_error_context.stack = frame->next;
        g_error_context.frame_count--;
        ppdb_base_mem_free(frame);
    }
    
    ppdb_base_mutex_unlock(g_error_mutex);
}

// Set error callback
ppdb_error_t ppdb_base_error_set_callback(ppdb_error_callback_t callback,
                                        void* user_data) {
    if (!g_error_mutex) return PPDB_ERR_NOT_INITIALIZED;
    
    ppdb_base_mutex_lock(g_error_mutex);
    g_error_context.callback = callback;
    g_error_context.callback_data = user_data;
    ppdb_base_mutex_unlock(g_error_mutex);
    
    return PPDB_OK;
}

// Get error statistics
ppdb_error_t ppdb_base_error_get_stats(ppdb_error_stats_t* stats) {
    if (!g_error_mutex || !stats) return PPDB_ERR_PARAM;
    
    ppdb_base_mutex_lock(g_error_mutex);
    memcpy(stats, &g_error_context.stats, sizeof(ppdb_error_stats_t));
    ppdb_base_mutex_unlock(g_error_mutex);
    
    return PPDB_OK;
}

// Reset error statistics
void ppdb_base_error_reset_stats(void) {
    if (!g_error_mutex) return;
    
    ppdb_base_mutex_lock(g_error_mutex);
    memset(&g_error_context.stats, 0, sizeof(ppdb_error_stats_t));
    ppdb_base_mutex_unlock(g_error_mutex);
}

// Set error context
ppdb_error_t ppdb_base_error_set_context(const ppdb_error_context_t* ctx) {
    if (!g_error_mutex || !ctx) return PPDB_ERR_PARAM;
    
    ppdb_base_mutex_lock(g_error_mutex);
    memcpy(&g_error_context, ctx, sizeof(ppdb_error_context_t));
    ppdb_base_mutex_unlock(g_error_mutex);
    
    return PPDB_OK;
}

// Get error context
const ppdb_error_context_t* ppdb_base_error_get_context(void) {
    return &g_error_context;
}

// Get current error code
ppdb_error_t ppdb_base_error_get_code(void) {
    return g_error_context.error;
}

// Check if code represents an error
bool ppdb_base_error_is_error(ppdb_error_t code) {
    return code != PPDB_OK;
}

// Convert error code to string
const char* ppdb_base_error_to_string(ppdb_error_t error) {
    switch (error) {
        case PPDB_OK:
            return "Success";
        case PPDB_ERR_PARAM:
            return "Invalid parameter";
        case PPDB_ERR_MEMORY:
            return "Memory allocation failed";
        case PPDB_ERR_IO:
            return "IO error";
        case PPDB_ERR_NOT_FOUND:
            return "Not found";
        case PPDB_ERR_EXISTS:
            return "Already exists";
        case PPDB_ERR_TIMEOUT:
            return "Operation timed out";
        case PPDB_ERR_BUSY:
            return "Resource busy";
        case PPDB_ERR_FULL:
            return "Resource full";
        case PPDB_ERR_EMPTY:
            return "Resource empty";
        case PPDB_ERR_NOT_INITIALIZED:
            return "Not initialized";
        case PPDB_ERR_INVALID_STATE:
            return "Invalid state";
        case PPDB_ERR_STACK_OVERFLOW:
            return "Stack overflow";
        case PPDB_ERR_CLOSED:
            return "Resource closed";
        default:
            return "Unknown error";
    }
}

//-----------------------------------------------------------------------------
// Base Initialization
//-----------------------------------------------------------------------------
ppdb_error_t ppdb_base_init(ppdb_base_t** base, const ppdb_base_config_t* config) {
    if (!base || !config) return PPDB_ERR_PARAM;
    
    ppdb_base_t* new_base = NULL;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_t), (void**)&new_base);
    if (err != PPDB_OK) return err;
    
    // Copy config
    memcpy(&new_base->config, config, sizeof(ppdb_base_config_t));
    
    // Initialize components
    new_base->initialized = true;
    new_base->lock = NULL;
    new_base->mempool = NULL;
    new_base->async_loop = NULL;
    new_base->io_manager = NULL;
    
    *base = new_base;
    return PPDB_OK;
}

void ppdb_base_cleanup(ppdb_base_t* base) {
    if (!base) return;
    
    if (base->initialized) {
        ppdb_base_error_cleanup();
        base->initialized = false;
    }
    
    ppdb_base_mem_free(base);
}

//-----------------------------------------------------------------------------
// Configuration
//-----------------------------------------------------------------------------
ppdb_error_t ppdb_base_config_init(ppdb_base_config_t* config) {
    if (!config) return PPDB_ERR_PARAM;
    
    config->memory_limit = 0;  // No limit
    config->thread_pool_size = 4;  // Default thread pool size
    config->thread_safe = true;
    config->enable_logging = true;
    config->log_level = PPDB_LOG_INFO;
    
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// String Operations
//-----------------------------------------------------------------------------
ppdb_error_t ppdb_base_string_equal(const char* s1, const char* s2, bool* out_result) {
    if (!s1 || !s2 || !out_result) return PPDB_ERR_PARAM;
    
    *out_result = (strcmp(s1, s2) == 0);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_string_hash(const char* str, size_t* out_hash) {
    if (!str || !out_hash) return PPDB_ERR_PARAM;
    
    size_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    *out_hash = hash;
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// File System Operations
//-----------------------------------------------------------------------------
ppdb_error_t ppdb_base_fs_exists(const char* path, bool* out_exists) {
    if (!path || !out_exists) return PPDB_ERR_PARAM;
    
    struct stat st;
    *out_exists = (stat(path, &st) == 0);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_fs_create_directory(const char* path) {
    if (!path) return PPDB_ERR_PARAM;
    
    if (mkdir(path, 0755) != 0) {
        if (errno == EEXIST) return PPDB_OK;
        return PPDB_ERR_IO;
    }
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Logging System
//-----------------------------------------------------------------------------
static FILE* log_file = NULL;
static int log_level = PPDB_LOG_INFO;

ppdb_error_t ppdb_base_log_init(const char* log_path) {
    if (!log_path) return PPDB_ERR_PARAM;
    
    log_file = fopen(log_path, "a");
    if (!log_file) return PPDB_ERR_IO;
    
    return PPDB_OK;
}

void ppdb_base_log_write(int level, const char* fmt, ...) {
    if (level < log_level || !log_file) return;
    
    time_t now;
    time(&now);
    struct tm* tm_info = localtime(&now);
    
    char time_str[26];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    
    fprintf(log_file, "[%s] ", time_str);
    
    va_list args;
    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    va_end(args);
    
    fprintf(log_file, "\n");
    fflush(log_file);
}

//-----------------------------------------------------------------------------
// Configuration System
//-----------------------------------------------------------------------------
typedef struct {
    char* key;
    char* value;
} config_entry_t;

static config_entry_t* config_entries = NULL;
static size_t config_count = 0;
static size_t config_capacity = 0;

ppdb_error_t ppdb_base_config_load(const char* config_path) {
    if (!config_path) return PPDB_ERR_PARAM;
    
    FILE* fp = fopen(config_path, "r");
    if (!fp) return PPDB_ERR_IO;
    
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        char* key = strtok(line, "=");
        char* value = strtok(NULL, "\n");
        
        if (key && value) {
            // Trim whitespace
            while (*key && isspace(*key)) key++;
            while (*value && isspace(*value)) value++;
            
            char* key_end = key + strlen(key) - 1;
            char* value_end = value + strlen(value) - 1;
            while (key_end > key && isspace(*key_end)) *key_end-- = 0;
            while (value_end > value && isspace(*value_end)) *value_end-- = 0;
            
            ppdb_base_config_set(key, value);
        }
    }
    
    fclose(fp);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_config_set(const char* key, const char* value) {
    if (!key || !value) return PPDB_ERR_PARAM;
    
    // Ensure we have enough capacity
    if (config_count >= config_capacity) {
        size_t new_capacity = config_capacity == 0 ? 16 : config_capacity * 2;
        void* new_entries;
        ppdb_error_t err = ppdb_base_mem_malloc(new_capacity * sizeof(config_entry_t), &new_entries);
        if (err != PPDB_OK) {
            return err;
        }
        
        if (config_entries) {
            memcpy(new_entries, config_entries, config_count * sizeof(config_entry_t));
            ppdb_base_mem_free(config_entries);
        }
        
        config_entries = (config_entry_t*)new_entries;
        config_capacity = new_capacity;
    }
    
    // Check if key already exists
    for (size_t i = 0; i < config_count; i++) {
        if (strcmp(config_entries[i].key, key) == 0) {
            // Update existing value
            void* new_value;
            ppdb_error_t err = ppdb_base_mem_malloc(strlen(value) + 1, &new_value);
            if (err != PPDB_OK) {
                return err;
            }
            
            strcpy((char*)new_value, value);
            ppdb_base_mem_free(config_entries[i].value);
            config_entries[i].value = (char*)new_value;
            return PPDB_OK;
        }
    }
    
    // Add new entry
    void* new_key;
    ppdb_error_t err = ppdb_base_mem_malloc(strlen(key) + 1, &new_key);
    if (err != PPDB_OK) {
        return err;
    }
    
    void* new_value;
    err = ppdb_base_mem_malloc(strlen(value) + 1, &new_value);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(new_key);
        return err;
    }
    
    strcpy((char*)new_key, key);
    strcpy((char*)new_value, value);
    
    config_entries[config_count].key = (char*)new_key;
    config_entries[config_count].value = (char*)new_value;
    config_count++;
    
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Time and System Utilities
//-----------------------------------------------------------------------------
ppdb_error_t ppdb_base_time_get_microseconds(uint64_t* out_time) {
    if (!out_time) return PPDB_ERR_PARAM;
    
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return PPDB_ERR_SYSTEM;
    }
    
    *out_time = (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_sys_get_cpu_count(uint32_t* out_count) {
    if (!out_count) return PPDB_ERR_PARAM;
    
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count < 0) {
        return PPDB_ERR_SYSTEM;
    }
    
    *out_count = (uint32_t)count;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_sys_get_page_size(size_t* out_size) {
    if (!out_size) return PPDB_ERR_PARAM;
    
    long size = sysconf(_SC_PAGESIZE);
    if (size < 0) {
        return PPDB_ERR_SYSTEM;
    }
    
    *out_size = (size_t)size;
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Memory Pool Implementation
//-----------------------------------------------------------------------------
ppdb_error_t ppdb_base_mempool_create(ppdb_base_mempool_t** pool, size_t block_size, size_t alignment) {
    if (!pool || block_size == 0) return PPDB_ERR_PARAM;
    
    ppdb_base_mempool_t* new_pool = NULL;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_mempool_t), (void**)&new_pool);
    if (err != PPDB_OK) return err;
    
    new_pool->head = NULL;
    new_pool->block_size = block_size;
    new_pool->alignment = alignment;
    
    // 初始化统计字段
    new_pool->total_allocated = 0;
    new_pool->total_used = 0;
    new_pool->total_blocks = 0;
    new_pool->total_allocations = 0;
    new_pool->total_frees = 0;
    new_pool->peak_allocated = 0;
    new_pool->peak_used = 0;
    
    // 创建互斥锁
    err = ppdb_base_mutex_create(&new_pool->lock);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(new_pool);
        return err;
    }
    
    *pool = new_pool;
    return PPDB_OK;
}

void* ppdb_base_mempool_alloc(ppdb_base_mempool_t* pool, size_t size) {
    if (!pool || size == 0) return NULL;
    
    ppdb_base_mutex_lock(pool->lock);
    
    // 查找可用块
    ppdb_base_mempool_block_t* block = pool->head;
    while (block) {
        if (block->size - block->used >= size) {
            void* ptr = (char*)block->data + block->used;
            block->used += size;
            
            // 更新统计信息
            pool->total_used += size;
            pool->total_allocations++;
            if (pool->total_used > pool->peak_used) {
                pool->peak_used = pool->total_used;
            }
            
            ppdb_base_mutex_unlock(pool->lock);
            return ptr;
        }
        block = block->next;
    }
    
    // 创建新块
    size_t block_size = size > pool->block_size ? size : pool->block_size;
    ppdb_base_mempool_block_t* new_block = NULL;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_mempool_block_t), (void**)&new_block);
    if (err != PPDB_OK) {
        ppdb_base_mutex_unlock(pool->lock);
        return NULL;
    }
    
    err = ppdb_base_mem_malloc(block_size, &new_block->data);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(new_block);
        ppdb_base_mutex_unlock(pool->lock);
        return NULL;
    }
    
    new_block->size = block_size;
    new_block->used = size;
    new_block->next = pool->head;
    pool->head = new_block;
    
    // 更新统计信息
    pool->total_allocated += block_size;
    pool->total_used += size;
    pool->total_blocks++;
    pool->total_allocations++;
    if (pool->total_allocated > pool->peak_allocated) {
        pool->peak_allocated = pool->total_allocated;
    }
    if (pool->total_used > pool->peak_used) {
        pool->peak_used = pool->total_used;
    }
    
    ppdb_base_mutex_unlock(pool->lock);
    return new_block->data;
}

void ppdb_base_mempool_free(ppdb_base_mempool_t* pool, void* ptr) {
    if (!pool || !ptr) return;
    
    ppdb_base_mutex_lock(pool->lock);
    
    // 查找包含此指针的块
    ppdb_base_mempool_block_t* block = pool->head;
    while (block) {
        if (ptr >= block->data && ptr < (char*)block->data + block->size) {
            // 找到块,更新统计信息
            pool->total_frees++;
            ppdb_base_mutex_unlock(pool->lock);
            return;
        }
        block = block->next;
    }
    
    ppdb_base_mutex_unlock(pool->lock);
}

void ppdb_base_mempool_get_stats(ppdb_base_mempool_t* pool, ppdb_base_mempool_stats_t* stats) {
    if (!pool || !stats) return;
    
    ppdb_base_mutex_lock(pool->lock);
    
    stats->total_allocated = pool->total_allocated;
    stats->total_used = pool->total_used;
    stats->total_blocks = pool->total_blocks;
    stats->total_allocations = pool->total_allocations;
    stats->total_frees = pool->total_frees;
    stats->peak_allocated = pool->peak_allocated;
    stats->peak_used = pool->peak_used;
    stats->block_size = pool->block_size;
    stats->alignment = pool->alignment;
    stats->fragmentation = pool->total_allocated - pool->total_used;
    
    ppdb_base_mutex_unlock(pool->lock);
}

ppdb_error_t ppdb_base_mempool_destroy(ppdb_base_mempool_t* pool) {
    if (!pool) return PPDB_ERR_PARAM;
    
    ppdb_base_mutex_lock(pool->lock);
    
    ppdb_base_mempool_block_t* block = pool->head;
    while (block) {
        ppdb_base_mempool_block_t* next = block->next;
        ppdb_base_mem_free(block->data);
        ppdb_base_mem_free(block);
        block = next;
    }
    
    ppdb_base_mutex_unlock(pool->lock);
    ppdb_base_mutex_destroy(pool->lock);
    ppdb_base_mem_free(pool);
    
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Aligned Memory Implementation
//-----------------------------------------------------------------------------
void* ppdb_base_aligned_alloc(size_t alignment, size_t size) {
    if (alignment == 0 || size == 0) return NULL;
    
    // Ensure alignment is a power of 2
    if ((alignment & (alignment - 1)) != 0) return NULL;
    
    // Add space for alignment and size storage
    size_t total_size = size + alignment + sizeof(size_t);
    
    // Allocate memory
    void* ptr = malloc(total_size);
    if (!ptr) return NULL;
    
    // Calculate aligned address
    size_t addr = (size_t)ptr + sizeof(size_t);
    size_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
    
    // Store original pointer before aligned address
    *((size_t*)(aligned_addr - sizeof(size_t))) = (size_t)ptr;
    
    return (void*)aligned_addr;
}

void ppdb_base_aligned_free(void* ptr) {
    if (!ptr) return;
    
    // Get original pointer
    void* original = (void*)*((size_t*)((size_t)ptr - sizeof(size_t)));
    free(original);
}

// Record an error occurrence
ppdb_error_t ppdb_base_error_record(ppdb_error_t code,
                                   ppdb_error_severity_t severity,
                                   ppdb_error_category_t category) {
    if (!g_error_mutex) return PPDB_ERR_STATE;
    
    ppdb_base_mutex_lock(g_error_mutex);
    
    // Update total error count
    g_error_context.stats.total_errors++;
    
    // Update severity stats
    if (severity < PPDB_ERROR_SEVERITY_FATAL) {
        g_error_context.stats.errors_by_severity[severity]++;
    }
    
    // Update category stats
    if (category < PPDB_ERROR_CATEGORY_USER) {
        g_error_context.stats.errors_by_category[category]++;
    }
    
    // Update timing information
    uint64_t current_time = ppdb_base_time_now();
    g_error_context.stats.last_error_time = current_time;
    
    // Calculate error-free time
    if (g_error_context.stats.error_free_time == 0) {
        g_error_context.stats.error_free_time = current_time;
    } else {
        uint64_t error_free = current_time - g_error_context.stats.error_free_time;
        g_error_context.stats.error_free_time = error_free;
    }
    
    ppdb_base_mutex_unlock(g_error_mutex);
    return PPDB_OK;
}

// Get current error statistics
ppdb_error_t ppdb_base_error_get_stats(ppdb_error_stats_t* stats) {
    if (!g_error_mutex || !stats) return PPDB_ERR_PARAM;
    
    ppdb_base_mutex_lock(g_error_mutex);
    memcpy(stats, &g_error_context.stats, sizeof(ppdb_error_stats_t));
    ppdb_base_mutex_unlock(g_error_mutex);
    
    return PPDB_OK;
}

// Reset error statistics
ppdb_error_t ppdb_base_error_reset_stats(void) {
    if (!g_error_mutex) return PPDB_ERR_STATE;
    
    ppdb_base_mutex_lock(g_error_mutex);
    memset(&g_error_context.stats, 0, sizeof(ppdb_error_stats_t));
    ppdb_base_mutex_unlock(g_error_mutex);
    
    return PPDB_OK;
}

// Analyze error trends
ppdb_error_t ppdb_base_error_analyze_trend(uint64_t window_size,
                                          ppdb_error_trend_t* trend) {
    if (!g_error_mutex || !trend || window_size == 0) return PPDB_ERR_PARAM;
    
    ppdb_base_mutex_lock(g_error_mutex);
    
    uint64_t current_time = ppdb_base_time_now();
    uint64_t window_start = current_time - window_size;
    
    // Initialize trend data
    memset(trend, 0, sizeof(ppdb_error_trend_t));
    trend->window_start = window_start;
    trend->window_size = window_size;
    
    // Count errors in window
    ppdb_error_frame_t* frame = g_error_context.stack;
    uint64_t max_errors_per_second = 0;
    uint64_t total_errors = 0;
    
    while (frame) {
        if (frame->timestamp >= window_start) {
            total_errors++;
            
            // Update severity
            if (frame->severity > trend->highest_severity) {
                trend->highest_severity = frame->severity;
            }
            
            // Update category frequency
            uint64_t category_count[PPDB_ERROR_CATEGORY_USER + 1] = {0};
            category_count[frame->category]++;
            
            // Find main category
            for (int i = 0; i <= PPDB_ERROR_CATEGORY_USER; i++) {
                if (category_count[i] > category_count[trend->main_category]) {
                    trend->main_category = i;
                }
            }
        }
        frame = frame->next;
    }
    
    trend->error_count = total_errors;
    trend->peak_error_rate = max_errors_per_second;
    trend->avg_error_rate = (double)total_errors / window_size;
    
    ppdb_base_mutex_unlock(g_error_mutex);
    return PPDB_OK;
}

