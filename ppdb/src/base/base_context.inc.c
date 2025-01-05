//-----------------------------------------------------------------------------
// Context Management Implementation
//-----------------------------------------------------------------------------

// Internal context structure
typedef struct ppdb_context_internal {
    uint64_t id;
    uint32_t state;
    ppdb_core_mutex_t* mutex;
    void* user_data;
    bool is_valid;
} ppdb_context_internal_t;

// Context pool
static ppdb_context_internal_t* g_context_pool = NULL;
static size_t g_context_pool_size = 0;
static ppdb_core_mutex_t* g_context_pool_mutex = NULL;

// Initialize context system
static ppdb_error_t context_system_init(void) {
    if (g_context_pool) return PPDB_OK;  // Already initialized
    
    ppdb_error_t err = ppdb_core_mutex_create(&g_context_pool_mutex);
    if (err != PPDB_OK) return err;
    
    g_context_pool_size = 1024;  // Initial pool size
    g_context_pool = ppdb_core_calloc(g_context_pool_size, sizeof(ppdb_context_internal_t));
    if (!g_context_pool) {
        ppdb_core_mutex_destroy(g_context_pool_mutex);
        g_context_pool_mutex = NULL;
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    
    return PPDB_OK;
}

// Create a new context
ppdb_error_t ppdb_context_create(ppdb_context_t* ctx) {
    if (!ctx) return PPDB_ERR_NULL_POINTER;
    
    ppdb_error_t err = context_system_init();
    if (err != PPDB_OK) return err;
    
    err = ppdb_core_mutex_lock(g_context_pool_mutex);
    if (err != PPDB_OK) return err;
    
    // Find free slot
    size_t i;
    for (i = 0; i < g_context_pool_size; i++) {
        if (!g_context_pool[i].is_valid) {
            // Initialize context
            g_context_pool[i].id = i + 1;  // 0 is invalid
            g_context_pool[i].state = 0;
            g_context_pool[i].user_data = NULL;
            err = ppdb_core_mutex_create(&g_context_pool[i].mutex);
            if (err != PPDB_OK) {
                ppdb_core_mutex_unlock(g_context_pool_mutex);
                return err;
            }
            g_context_pool[i].is_valid = true;
            *ctx = g_context_pool[i].id;
            ppdb_core_mutex_unlock(g_context_pool_mutex);
            return PPDB_OK;
        }
    }
    
    ppdb_core_mutex_unlock(g_context_pool_mutex);
    return PPDB_ERR_FULL;
}

// Destroy a context
void ppdb_context_destroy(ppdb_context_t ctx) {
    if (ctx == 0) return;
    
    ppdb_core_mutex_lock(g_context_pool_mutex);
    
    size_t idx = ctx - 1;
    if (idx < g_context_pool_size && g_context_pool[idx].is_valid) {
        ppdb_core_mutex_destroy(g_context_pool[idx].mutex);
        memset(&g_context_pool[idx], 0, sizeof(ppdb_context_internal_t));
    }
    
    ppdb_core_mutex_unlock(g_context_pool_mutex);
}

// Get context state
ppdb_error_t ppdb_context_get_state(ppdb_context_t ctx, uint32_t* state) {
    if (ctx == 0 || !state) return PPDB_ERR_NULL_POINTER;
    
    size_t idx = ctx - 1;
    if (idx >= g_context_pool_size || !g_context_pool[idx].is_valid) {
        return PPDB_ERR_INVALID_ARGUMENT;
    }
    
    ppdb_error_t err = ppdb_core_mutex_lock(g_context_pool[idx].mutex);
    if (err != PPDB_OK) return err;
    
    *state = g_context_pool[idx].state;
    
    return ppdb_core_mutex_unlock(g_context_pool[idx].mutex);
}
