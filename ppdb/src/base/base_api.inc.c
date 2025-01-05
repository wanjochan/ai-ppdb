#ifndef PPDB_BASE_API_INC_C
#define PPDB_BASE_API_INC_C

ppdb_error_t ppdb_create(ppdb_ctx_t* ctx, const ppdb_options_t* options) {
    if (!ctx) return PPDB_ERR_NULL_POINTER;
    
    // 创建上下文
    ppdb_context_t* context;
    ppdb_error_t err = ppdb_context_create(&context);
    if (err != PPDB_OK) return err;
    
    // 创建内存池
    ppdb_mempool_t* pool = NULL;
    err = ppdb_mempool_create(&pool, 4096, 16);  // 4KB blocks, 16-byte alignment
    if (err != PPDB_OK) {
        ppdb_context_destroy(context);
        return err;
    }
    context->pool = pool;
    
    // 创建基础结构
    ppdb_base_t* base = NULL;
    err = ppdb_base_init(&base);
    if (err != PPDB_OK) {
        ppdb_context_destroy(context);
        return err;
    }
    context->base = base;
    
    // 复制配置
    if (options) {
        memcpy(&context->options, options, sizeof(ppdb_options_t));
    } else {
        // 默认配置
        context->options.db_path = ":memory:";
        context->options.cache_size = 1024 * 1024 * 16;  // 16MB
        context->options.max_readers = 32;
        context->options.sync_writes = false;
        context->options.flush_period_ms = 1000;
    }
    
    // 返回上下文句柄
    *ctx = (ppdb_ctx_t)context;
    return PPDB_OK;
}

ppdb_error_t ppdb_destroy(ppdb_ctx_t ctx) {
    if (!ctx) return PPDB_ERR_NULL_POINTER;
    
    ppdb_context_t* context = (ppdb_context_t*)ctx;
    
    // 销毁基础结构
    if (context->base) {
        ppdb_base_destroy(context->base);
        context->base = NULL;
    }
    
    // 销毁上下文 (内存池会在context_destroy中销毁)
    ppdb_context_destroy(context);
    
    return PPDB_OK;
}

#endif // PPDB_BASE_API_INC_C 