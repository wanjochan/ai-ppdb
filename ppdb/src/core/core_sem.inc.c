//-----------------------------------------------------------------------------
// Semaphore Implementation
//-----------------------------------------------------------------------------

struct ppdb_core_sem {
    #ifdef _WIN32
    HANDLE handle;
    #else
    sem_t sem;
    #endif
    atomic_size_t value;
    ppdb_core_mutex_t* mutex;
    ppdb_core_cond_t* cond;
    bool use_native;
};

ppdb_error_t ppdb_core_sem_create(ppdb_core_sem_t** sem, size_t initial_value) {
    if (!sem) return PPDB_ERR_NULL_POINTER;

    *sem = ppdb_core_alloc(sizeof(ppdb_core_sem_t));
    if (!*sem) return PPDB_ERR_OUT_OF_MEMORY;

    memset(*sem, 0, sizeof(ppdb_core_sem_t));
    atomic_init(&(*sem)->value, initial_value);

    #ifdef _WIN32
    (*sem)->handle = CreateSemaphore(NULL, initial_value, INT_MAX, NULL);
    if (!(*sem)->handle) {
        ppdb_core_free(*sem);
        return PPDB_ERR_INTERNAL;
    }
    (*sem)->use_native = true;
    #else
    if (sem_init(&(*sem)->sem, 0, initial_value) != 0) {
        // 如果原生信号量失败，使用条件变量实现
        ppdb_error_t err = ppdb_core_mutex_create(&(*sem)->mutex);
        if (err != PPDB_OK) {
            ppdb_core_free(*sem);
            return err;
        }

        err = ppdb_core_cond_create(&(*sem)->cond);
        if (err != PPDB_OK) {
            ppdb_core_mutex_destroy((*sem)->mutex);
            ppdb_core_free(*sem);
            return err;
        }
        (*sem)->use_native = false;
    } else {
        (*sem)->use_native = true;
    }
    #endif

    return PPDB_OK;
}

ppdb_error_t ppdb_core_sem_destroy(ppdb_core_sem_t* sem) {
    if (!sem) return PPDB_ERR_NULL_POINTER;

    if (sem->use_native) {
        #ifdef _WIN32
        CloseHandle(sem->handle);
        #else
        sem_destroy(&sem->sem);
        #endif
    } else {
        if (sem->mutex) ppdb_core_mutex_destroy(sem->mutex);
        if (sem->cond) ppdb_core_cond_destroy(sem->cond);
    }

    ppdb_core_free(sem);
    return PPDB_OK;
}

ppdb_error_t ppdb_core_sem_wait(ppdb_core_sem_t* sem) {
    if (!sem) return PPDB_ERR_NULL_POINTER;

    if (sem->use_native) {
        #ifdef _WIN32
        if (WaitForSingleObject(sem->handle, INFINITE) != WAIT_OBJECT_0) {
            return PPDB_ERR_INTERNAL;
        }
        #else
        if (sem_wait(&sem->sem) != 0) {
            return PPDB_ERR_INTERNAL;
        }
        #endif
    } else {
        ppdb_core_mutex_lock(sem->mutex);
        while (atomic_load(&sem->value) == 0) {
            ppdb_core_cond_wait(sem->cond, sem->mutex);
        }
        atomic_fetch_sub(&sem->value, 1);
        ppdb_core_mutex_unlock(sem->mutex);
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_core_sem_trywait(ppdb_core_sem_t* sem) {
    if (!sem) return PPDB_ERR_NULL_POINTER;

    if (sem->use_native) {
        #ifdef _WIN32
        if (WaitForSingleObject(sem->handle, 0) != WAIT_OBJECT_0) {
            return PPDB_ERR_WOULD_BLOCK;
        }
        #else
        if (sem_trywait(&sem->sem) != 0) {
            return PPDB_ERR_WOULD_BLOCK;
        }
        #endif
    } else {
        ppdb_core_mutex_lock(sem->mutex);
        if (atomic_load(&sem->value) == 0) {
            ppdb_core_mutex_unlock(sem->mutex);
            return PPDB_ERR_WOULD_BLOCK;
        }
        atomic_fetch_sub(&sem->value, 1);
        ppdb_core_mutex_unlock(sem->mutex);
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_core_sem_timedwait(ppdb_core_sem_t* sem, uint32_t timeout_ms) {
    if (!sem) return PPDB_ERR_NULL_POINTER;

    if (sem->use_native) {
        #ifdef _WIN32
        if (WaitForSingleObject(sem->handle, timeout_ms) != WAIT_OBJECT_0) {
            return PPDB_ERR_TIMEOUT;
        }
        #else
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000;
        }
        if (sem_timedwait(&sem->sem, &ts) != 0) {
            return PPDB_ERR_TIMEOUT;
        }
        #endif
    } else {
        ppdb_core_mutex_lock(sem->mutex);
        while (atomic_load(&sem->value) == 0) {
            ppdb_error_t err = ppdb_core_cond_timedwait(sem->cond, sem->mutex, timeout_ms);
            if (err != PPDB_OK) {
                ppdb_core_mutex_unlock(sem->mutex);
                return err;
            }
        }
        atomic_fetch_sub(&sem->value, 1);
        ppdb_core_mutex_unlock(sem->mutex);
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_core_sem_post(ppdb_core_sem_t* sem) {
    if (!sem) return PPDB_ERR_NULL_POINTER;

    if (sem->use_native) {
        #ifdef _WIN32
        if (!ReleaseSemaphore(sem->handle, 1, NULL)) {
            return PPDB_ERR_INTERNAL;
        }
        #else
        if (sem_post(&sem->sem) != 0) {
            return PPDB_ERR_INTERNAL;
        }
        #endif
    } else {
        ppdb_core_mutex_lock(sem->mutex);
        atomic_fetch_add(&sem->value, 1);
        ppdb_core_cond_signal(sem->cond);
        ppdb_core_mutex_unlock(sem->mutex);
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_core_sem_getvalue(ppdb_core_sem_t* sem, size_t* value) {
    if (!sem || !value) return PPDB_ERR_NULL_POINTER;

    if (sem->use_native) {
        #ifdef _WIN32
        LONG prev_count;
        if (!ReleaseSemaphore(sem->handle, 0, &prev_count)) {
            return PPDB_ERR_INTERNAL;
        }
        *value = prev_count;
        #else
        int val;
        if (sem_getvalue(&sem->sem, &val) != 0) {
            return PPDB_ERR_INTERNAL;
        }
        *value = val;
        #endif
    } else {
        *value = atomic_load(&sem->value);
    }

    return PPDB_OK;
}
