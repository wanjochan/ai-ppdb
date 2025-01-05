#ifndef PPDB_BASE_CURSOR_INC_C
#define PPDB_BASE_CURSOR_INC_C

// Cursor pool
#define PPDB_CURSOR_POOL_SIZE 256

typedef struct ppdb_cursor_internal {
    ppdb_cursor_t cursor;
    bool used;
    uint32_t id;
} ppdb_cursor_internal_t;

static ppdb_cursor_internal_t* g_cursor_pool = NULL;
static size_t g_cursor_pool_size = PPDB_CURSOR_POOL_SIZE;
static ppdb_core_mutex_t* g_cursor_pool_mutex = NULL;

static ppdb_error_t cursor_system_init(void) {
    if (g_cursor_pool) return PPDB_ERR_EXISTS;

    // Create pool mutex
    ppdb_error_t err = ppdb_core_mutex_create(&g_cursor_pool_mutex);
    if (err != PPDB_OK) return err;

    // Allocate cursor pool
    g_cursor_pool = (ppdb_cursor_internal_t*)ppdb_aligned_alloc(
        g_cursor_pool_size * sizeof(ppdb_cursor_internal_t));
    if (!g_cursor_pool) {
        ppdb_core_mutex_destroy(g_cursor_pool_mutex);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // Initialize pool
    memset(g_cursor_pool, 0, g_cursor_pool_size * sizeof(ppdb_cursor_internal_t));
    return PPDB_OK;
}

static void cursor_system_cleanup(void) __attribute__((destructor));
static void cursor_system_cleanup(void) {
    if (g_cursor_pool) {
        ppdb_aligned_free(g_cursor_pool);
        g_cursor_pool = NULL;
    }

    if (g_cursor_pool_mutex) {
        ppdb_core_mutex_destroy(g_cursor_pool_mutex);
        g_cursor_pool_mutex = NULL;
    }
}

ppdb_error_t ppdb_cursor_create(ppdb_ctx_t ctx_handle, ppdb_cursor_t** cursor) {
    if (!cursor) return PPDB_ERR_NULL_POINTER;

    ppdb_context_t* ctx = ppdb_context_get(ctx_handle);
    if (!ctx) return PPDB_ERR_INVALID_ARGUMENT;

    // Initialize cursor system if needed
    ppdb_error_t err = cursor_system_init();
    if (err != PPDB_OK) return err;

    // Lock pool
    err = ppdb_core_mutex_lock(g_cursor_pool_mutex);
    if (err != PPDB_OK) return err;

    // Find free slot
    ppdb_cursor_internal_t* cur = NULL;
    for (size_t i = 0; i < g_cursor_pool_size; i++) {
        if (!g_cursor_pool[i].used) {
            cur = &g_cursor_pool[i];
            cur->used = true;
            cur->id = (uint32_t)i + 1;
            break;
        }
    }

    // Unlock pool
    ppdb_core_mutex_unlock(g_cursor_pool_mutex);

    if (!cur) return PPDB_ERR_FULL;

    // Initialize cursor
    memset(&cur->cursor, 0, sizeof(ppdb_cursor_t));
    cur->cursor.ctx = ctx;
    cur->cursor.flags = 0;
    cur->cursor.user_data = NULL;

    // Create cursor mutex
    err = ppdb_core_mutex_create(&cur->cursor.mutex);
    if (err != PPDB_OK) {
        cur->used = false;
        return err;
    }

    *cursor = &cur->cursor;
    return PPDB_OK;
}

void ppdb_cursor_destroy(ppdb_cursor_t* cursor) {
    if (!cursor || !g_cursor_pool) return;

    ppdb_core_mutex_lock(g_cursor_pool_mutex);

    // Find cursor in pool
    for (size_t i = 0; i < g_cursor_pool_size; i++) {
        if (g_cursor_pool[i].used && &g_cursor_pool[i].cursor == cursor) {
            // Destroy cursor mutex
            if (cursor->mutex) {
                ppdb_core_mutex_destroy(cursor->mutex);
            }

            // Clear cursor data
            memset(&g_cursor_pool[i], 0, sizeof(ppdb_cursor_internal_t));
            break;
        }
    }

    ppdb_core_mutex_unlock(g_cursor_pool_mutex);
}

ppdb_error_t ppdb_cursor_next(ppdb_cursor_t* cursor, ppdb_data_t* key, ppdb_data_t* value) {
    if (!cursor || !key || !value) return PPDB_ERR_NULL_POINTER;

    // Lock cursor
    ppdb_error_t err = ppdb_core_mutex_lock(cursor->mutex);
    if (err != PPDB_OK) return err;

    // TODO: Implement cursor iteration
    err = PPDB_ERR_NOT_IMPLEMENTED;

    // Unlock cursor
    ppdb_core_mutex_unlock(cursor->mutex);

    return err;
}

#endif // PPDB_BASE_CURSOR_INC_C
