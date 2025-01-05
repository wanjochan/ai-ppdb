// Base synchronization primitives implementation
#include <cosmopolitan.h>

struct ppdb_sync_s {
    pthread_mutex_t mutex;
    pthread_rwlock_t rwlock;
    int type;  // 0: mutex, 1: rwlock
};

ppdb_error_t ppdb_sync_create(ppdb_sync_t** sync, int type) {
    ppdb_sync_t* s = malloc(sizeof(ppdb_sync_t));
    if (!s) return PPDB_ERROR_OOM;
    
    s->type = type;
    if (type == 0) {
        if (pthread_mutex_init(&s->mutex, NULL) != 0) {
            free(s);
            return PPDB_ERROR_SYNC;
        }
    } else {
        if (pthread_rwlock_init(&s->rwlock, NULL) != 0) {
            free(s);
            return PPDB_ERROR_SYNC;
        }
    }
    
    *sync = s;
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_destroy(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERROR_INVALID;
    
    if (sync->type == 0) {
        pthread_mutex_destroy(&sync->mutex);
    } else {
        pthread_rwlock_destroy(&sync->rwlock);
    }
    
    free(sync);
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERROR_INVALID;
    if (sync->type != 0) return PPDB_ERROR_INVALID;
    
    if (pthread_mutex_lock(&sync->mutex) != 0) {
        return PPDB_ERROR_SYNC;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERROR_INVALID;
    if (sync->type != 0) return PPDB_ERROR_INVALID;
    
    if (pthread_mutex_unlock(&sync->mutex) != 0) {
        return PPDB_ERROR_SYNC;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERROR_INVALID;
    if (sync->type != 1) return PPDB_ERROR_INVALID;
    
    if (pthread_rwlock_rdlock(&sync->rwlock) != 0) {
        return PPDB_ERROR_SYNC;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERROR_INVALID;
    if (sync->type != 1) return PPDB_ERROR_INVALID;
    
    if (pthread_rwlock_unlock(&sync->rwlock) != 0) {
        return PPDB_ERROR_SYNC;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_write_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERROR_INVALID;
    if (sync->type != 1) return PPDB_ERROR_INVALID;
    
    if (pthread_rwlock_wrlock(&sync->rwlock) != 0) {
        return PPDB_ERROR_SYNC;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_write_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERROR_INVALID;
    if (sync->type != 1) return PPDB_ERROR_INVALID;
    
    if (pthread_rwlock_unlock(&sync->rwlock) != 0) {
        return PPDB_ERROR_SYNC;
    }
    return PPDB_OK;
} 