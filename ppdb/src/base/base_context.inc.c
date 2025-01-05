#ifndef PPDB_BASE_CONTEXT_INC_C
#define PPDB_BASE_CONTEXT_INC_C

#define CONTEXT_ALIGNMENT 16  // 添加对齐常量

typedef struct ppdb_context_internal {
    ppdb_context_t ctx;
    bool used;
    uint32_t id;
} ppdb_context_internal_t;

static ppdb_context_internal_t* g_context_pool = NULL;
static size_t g_context_pool_size = 256;  // 默认池大小
static ppdb_engine_mutex_t* g_context_pool_mutex = NULL;

static ppdb_error_t context_system_init(void) {
    if (g_context_pool) return PPDB_ERR_EXISTS;

    // Create pool mutex
    ppdb_error_t err = ppdb_engine_mutex_create(&g_context_pool_mutex);
    if (err != PPDB_OK) return err;

    // Allocate context pool
    g_context_pool = (ppdb_context_internal_t*)ppdb_aligned_alloc(CONTEXT_ALIGNMENT,
        g_context_pool_size * sizeof(ppdb_context_internal_t));
    if (!g_context_pool) {
        ppdb_engine_mutex_destroy(g_context_pool_mutex);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // Initialize pool
    memset(g_context_pool, 0, g_context_pool_size * sizeof(ppdb_context_internal_t));
    return PPDB_OK;
}

static void context_system_cleanup(void) __attribute__((destructor));
static void context_system_cleanup(void) {
    if (g_context_pool) {
        ppdb_aligned_free(g_context_pool);
        g_context_pool = NULL;
    }

    if (g_context_pool_mutex) {
        ppdb_engine_mutex_destroy(g_context_pool_mutex);
        g_context_pool_mutex = NULL;
    }
}

ppdb_error_t ppdb_context_create(ppdb_context_t** ctx) {
    if (!ctx) return PPDB_ERR_NULL_POINTER;
    *ctx = NULL;

    // Initialize context system if needed
    ppdb_error_t err = context_system_init();
    if (err != PPDB_OK) return err;

    // Lock pool
    err = ppdb_engine_mutex_lock(g_context_pool_mutex);
    if (err != PPDB_OK) return err;

    // Find free slot
    ppdb_context_internal_t* context = NULL;
    for (size_t i = 0; i < g_context_pool_size; i++) {
        if (!g_context_pool[i].used) {
            context = &g_context_pool[i];
            context->used = true;
            context->id = (uint32_t)i + 1;
            break;
        }
    }

    // Unlock pool
    ppdb_engine_mutex_unlock(g_context_pool_mutex);

    if (!context) return PPDB_ERR_FULL;

    // Initialize context
    memset(&context->ctx, 0, sizeof(ppdb_context_t));
    context->ctx.pool = NULL;  // Will be initialized by caller
    context->ctx.flags = 0;
    context->ctx.user_data = NULL;

    *ctx = &context->ctx;
    return PPDB_OK;
}

void ppdb_context_destroy(ppdb_context_t* ctx) {
    if (!ctx || !g_context_pool) return;

    ppdb_engine_mutex_lock(g_context_pool_mutex);

    // Find context in pool
    ppdb_context_internal_t* context = NULL;
    for (size_t i = 0; i < g_context_pool_size; i++) {
        if (g_context_pool[i].used && &g_context_pool[i].ctx == ctx) {
            context = &g_context_pool[i];
            break;
        }
    }

    if (context) {
        // 销毁内存池
        if (context->ctx.pool) {
            ppdb_mempool_destroy(context->ctx.pool);
            context->ctx.pool = NULL;
        }
        
        // 清理上下文
        memset(&context->ctx, 0, sizeof(ppdb_context_t));
        context->used = false;
        context->id = 0;
    }

    ppdb_engine_mutex_unlock(g_context_pool_mutex);
}

ppdb_context_t* ppdb_context_get(ppdb_ctx_t ctx_handle) {
    if (!ctx_handle || !g_context_pool) return NULL;

    size_t index = (size_t)(ctx_handle - 1);
    if (index >= g_context_pool_size) return NULL;

    ppdb_context_internal_t* context = &g_context_pool[index];
    if (!context->used || context->id != ctx_handle) return NULL;

    return &context->ctx;
}

#endif // PPDB_BASE_CONTEXT_INC_C
