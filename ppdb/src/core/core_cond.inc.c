//-----------------------------------------------------------------------------
// Condition Variable Implementation 
//-----------------------------------------------------------------------------

struct ppdb_core_cond {
    pthread_cond_t cond;
    ppdb_core_mutex_t* mutex;
    atomic_size_t waiters;
};

ppdb_error_t ppdb_core_cond_create(ppdb_core_cond_t** cond) {
    if (!cond) return PPDB_ERR_NULL_POINTER;

    *cond = ppdb_core_alloc(sizeof(ppdb_core_cond_t));
    if (!*cond) return PPDB_ERR_OUT_OF_MEMORY;

    memset(*cond, 0, sizeof(ppdb_core_cond_t));
    atomic_init(&(*cond)->waiters, 0);

    if (pthread_cond_init(&(*cond)->cond, NULL) != 0) {
        ppdb_core_free(*cond);
        *cond = NULL;
        return PPDB_ERR_INTERNAL;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_core_cond_destroy(ppdb_core_cond_t* cond) {
    if (!cond) return PPDB_ERR_NULL_POINTER;

    if (pthread_cond_destroy(&cond->cond) != 0) {
        return PPDB_ERR_INTERNAL;
    }

    ppdb_core_free(cond);
    return PPDB_OK;
}

ppdb_error_t ppdb_core_cond_wait(ppdb_core_cond_t* cond, ppdb_core_mutex_t* mutex) {
    if (!cond || !mutex) return PPDB_ERR_NULL_POINTER;

    atomic_fetch_add(&cond->waiters, 1);
    int ret = pthread_cond_wait(&cond->cond, &mutex->mutex);
    atomic_fetch_sub(&cond->waiters, 1);

    if (ret != 0) return PPDB_ERR_INTERNAL;
    return PPDB_OK;
}

ppdb_error_t ppdb_core_cond_timedwait(ppdb_core_cond_t* cond, 
                                     ppdb_core_mutex_t* mutex,
                                     uint32_t timeout_ms) {
    if (!cond || !mutex) return PPDB_ERR_NULL_POINTER;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }

    atomic_fetch_add(&cond->waiters, 1);
    int ret = pthread_cond_timedwait(&cond->cond, &mutex->mutex, &ts);
    atomic_fetch_sub(&cond->waiters, 1);

    if (ret == ETIMEDOUT) return PPDB_ERR_TIMEOUT;
    if (ret != 0) return PPDB_ERR_INTERNAL;
    return PPDB_OK;
}

ppdb_error_t ppdb_core_cond_signal(ppdb_core_cond_t* cond) {
    if (!cond) return PPDB_ERR_NULL_POINTER;

    if (atomic_load(&cond->waiters) > 0) {
        if (pthread_cond_signal(&cond->cond) != 0) {
            return PPDB_ERR_INTERNAL;
        }
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_core_cond_broadcast(ppdb_core_cond_t* cond) {
    if (!cond) return PPDB_ERR_NULL_POINTER;

    if (atomic_load(&cond->waiters) > 0) {
        if (pthread_cond_broadcast(&cond->cond) != 0) {
            return PPDB_ERR_INTERNAL;
        }
    }
    return PPDB_OK;
}
