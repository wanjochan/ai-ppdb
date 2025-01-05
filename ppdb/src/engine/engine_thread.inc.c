//-----------------------------------------------------------------------------
// Thread Management Implementation
//-----------------------------------------------------------------------------

struct ppdb_engine_thread {
    pthread_t thread;
    void* (*start_routine)(void*);
    void* arg;
    bool detached;
};

ppdb_error_t ppdb_engine_thread_create(ppdb_engine_thread_t** thread,
                                    void* (*start_routine)(void*),
                                    void* arg) {
    if (!thread || !start_routine) return PPDB_ERR_NULL_POINTER;
    
    *thread = ppdb_engine_alloc(sizeof(ppdb_engine_thread_t));
    if (!*thread) return PPDB_ERR_OUT_OF_MEMORY;
    
    (*thread)->start_routine = start_routine;
    (*thread)->arg = arg;
    (*thread)->detached = false;
    
    if (pthread_create(&(*thread)->thread, NULL, start_routine, arg) != 0) {
        ppdb_engine_free(*thread);
        *thread = NULL;
        return PPDB_ERR_INTERNAL;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_thread_join(ppdb_engine_thread_t* thread, void** retval) {
    if (!thread) return PPDB_ERR_NULL_POINTER;
    if (thread->detached) return PPDB_ERR_INVALID_STATE;
    
    if (pthread_join(thread->thread, retval) != 0) {
        return PPDB_ERR_INTERNAL;
    }
    
    ppdb_engine_free(thread);
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_thread_detach(ppdb_engine_thread_t* thread) {
    if (!thread) return PPDB_ERR_NULL_POINTER;
    if (thread->detached) return PPDB_ERR_INVALID_STATE;
    
    if (pthread_detach(thread->thread) != 0) {
        return PPDB_ERR_INTERNAL;
    }
    
    thread->detached = true;
    return PPDB_OK;
}
