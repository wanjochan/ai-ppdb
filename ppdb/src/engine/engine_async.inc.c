/*
 * engine_async.inc.c - Engine Async Operations Implementation
 */

// Async operations
ppdb_error_t ppdb_engine_async_schedule(ppdb_engine_t* engine, ppdb_engine_async_fn fn, void* arg, ppdb_base_async_handle_t** handle) {
    if (!engine || !fn || !handle) return PPDB_ENGINE_ERR_PARAM;

    // Create a wrapper function that will be called by base layer
    ppdb_base_async_func_t wrapper_fn = (ppdb_base_async_func_t)fn;

    // Schedule the task using base layer
    ppdb_error_t err = ppdb_base_async_schedule(engine->base, wrapper_fn, arg, handle);
    if (err) return PPDB_ENGINE_ERR_ASYNC;

    return PPDB_OK;
}

void ppdb_engine_async_cancel(ppdb_base_async_handle_t* handle) {
    if (!handle) return;
    ppdb_base_async_cancel(handle);
}

void ppdb_engine_yield(void) {
    ppdb_base_yield();
}

void ppdb_engine_sleep(uint32_t milliseconds) {
    ppdb_base_sleep(milliseconds);
} 