#ifndef PPDB_BASE_CURSOR_INC_C
#define PPDB_BASE_CURSOR_INC_C

#define CURSOR_ALIGNMENT 16  // 添加对齐常量

typedef struct ppdb_cursor_internal_s {
    ppdb_cursor_t cursor;
    struct ppdb_cursor_internal_s* next;
} ppdb_cursor_internal_t;

static ppdb_cursor_internal_t* g_cursor_pool = NULL;
static ppdb_engine_mutex_t* g_cursor_mutex = NULL;
static bool g_cursor_initialized = false;

// Initialize cursor system
static ppdb_error_t cursor_system_init(void) {
    if (g_cursor_initialized) return PPDB_OK;

    // Create mutex
    ppdb_error_t err = ppdb_engine_mutex_create(&g_cursor_mutex);
    if (err != PPDB_OK) return err;

    // Allocate initial pool
    g_cursor_pool = (ppdb_cursor_internal_t*)ppdb_aligned_alloc(CURSOR_ALIGNMENT, 
        sizeof(ppdb_cursor_internal_t) * 16);  // Initial pool size
    if (!g_cursor_pool) {
        ppdb_engine_mutex_destroy(g_cursor_mutex);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // Initialize pool
    for (int i = 0; i < 15; i++) {
        g_cursor_pool[i].next = &g_cursor_pool[i + 1];
    }
    g_cursor_pool[15].next = NULL;

    g_cursor_initialized = true;
    return PPDB_OK;
}

// Create cursor
ppdb_error_t ppdb_cursor_create(ppdb_ctx_t ctx_handle, ppdb_cursor_t** cursor) {
    if (!cursor) return PPDB_ERR_NULL_POINTER;
    *cursor = NULL;

    // Initialize system if needed
    ppdb_error_t err = cursor_system_init();
    if (err != PPDB_OK) return err;

    // Lock pool
    ppdb_engine_mutex_lock(g_cursor_mutex);

    // Get cursor from pool
    ppdb_cursor_internal_t* internal = g_cursor_pool;
    if (internal) {
        g_cursor_pool = internal->next;
    } else {
        // Allocate new cursor
        internal = (ppdb_cursor_internal_t*)ppdb_aligned_alloc(CURSOR_ALIGNMENT, 
            sizeof(ppdb_cursor_internal_t));
        if (!internal) {
            ppdb_engine_mutex_unlock(g_cursor_mutex);
            return PPDB_ERR_OUT_OF_MEMORY;
        }
    }

    // Initialize cursor
    memset(internal, 0, sizeof(ppdb_cursor_internal_t));
    internal->cursor.ctx = ppdb_context_get(ctx_handle);
    if (!internal->cursor.ctx) {
        ppdb_aligned_free(internal);
        ppdb_engine_mutex_unlock(g_cursor_mutex);
        return PPDB_ERR_INVALID_ARGUMENT;
    }

    *cursor = &internal->cursor;
    ppdb_engine_mutex_unlock(g_cursor_mutex);
    return PPDB_OK;
}

// Destroy cursor
void ppdb_cursor_destroy(ppdb_cursor_t* cursor) {
    if (!cursor) return;

    // Get internal cursor
    ppdb_cursor_internal_t* internal = (ppdb_cursor_internal_t*)cursor;

    // Lock pool
    ppdb_engine_mutex_lock(g_cursor_mutex);

    // Return to pool
    internal->next = g_cursor_pool;
    g_cursor_pool = internal;

    ppdb_engine_mutex_unlock(g_cursor_mutex);
}

// Get next key-value pair
ppdb_error_t ppdb_cursor_next(ppdb_cursor_t* cursor, ppdb_data_t* key, ppdb_data_t* value) {
    if (!cursor || !key || !value) return PPDB_ERR_NULL_POINTER;
    if (!cursor->ctx) return PPDB_ERR_INVALID_STATE;

    // TODO: Implement cursor iteration
    return PPDB_ERR_NOT_IMPLEMENTED;
}

#endif // PPDB_BASE_CURSOR_INC_C
