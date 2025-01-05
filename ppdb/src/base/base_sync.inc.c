#ifndef PPDB_BASE_SYNC_INC_C
#define PPDB_BASE_SYNC_INC_C

ppdb_error_t ppdb_core_mutex_create(ppdb_core_mutex_t** mutex) {
    if (!mutex) return PPDB_ERR_NULL_POINTER;

    ppdb_core_mutex_t* m = (ppdb_core_mutex_t*)ppdb_aligned_alloc(sizeof(ppdb_core_mutex_t));
    if (!m) return PPDB_ERR_OUT_OF_MEMORY;

    if (pthread_mutex_init(&m->mutex, NULL) != 0) {
        ppdb_aligned_free(m);
        return PPDB_ERR_BUSY;
    }

    *mutex = m;
    return PPDB_OK;
}

void ppdb_core_mutex_destroy(ppdb_core_mutex_t* mutex) {
    if (!mutex) return;

    pthread_mutex_destroy(&mutex->mutex);
    ppdb_aligned_free(mutex);
}

ppdb_error_t ppdb_core_mutex_lock(ppdb_core_mutex_t* mutex) {
    if (!mutex) return PPDB_ERR_NULL_POINTER;

    if (pthread_mutex_lock(&mutex->mutex) != 0) {
        return PPDB_ERR_BUSY;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_core_mutex_unlock(ppdb_core_mutex_t* mutex) {
    if (!mutex) return PPDB_ERR_NULL_POINTER;

    if (pthread_mutex_unlock(&mutex->mutex) != 0) {
        return PPDB_ERR_BUSY;
    }

    return PPDB_OK;
}

#endif // PPDB_BASE_SYNC_INC_C 