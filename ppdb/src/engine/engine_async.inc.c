/*
 * engine_async.inc.c - Engine Async Operations Implementation
 */

// Async operations
ppdb_error_t ppdb_engine_async_schedule(ppdb_engine_t* engine, ppdb_engine_async_fn fn, void* arg, ppdb_base_async_handle_t** handle) {
    if (!engine || !fn || !handle) return PPDB_ENGINE_ERR_PARAM;

    // Create async task
    ppdb_base_async_task_t* task = malloc(sizeof(ppdb_base_async_task_t));
    if (!task) return PPDB_ENGINE_ERR_INIT;

    // Initialize task
    task->fn = fn;
    task->arg = arg;
    task->cancelled = false;

    // Schedule task
    ppdb_error_t err = ppdb_base_async_schedule(engine->base, task, handle);
    if (err != PPDB_OK) {
        free(task);
        return err;
    }

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