/*
 * base_rwlock.inc.c - Read-Write Lock Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

ppdb_error_t ppdb_base_rwlock_create(ppdb_base_rwlock_t** rwlock) {
    ppdb_base_rwlock_t* new_rwlock;

    if (!rwlock) {
        return PPDB_BASE_ERR_PARAM;
    }

    new_rwlock = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_base_rwlock_t));
    if (!new_rwlock) {
        return PPDB_BASE_ERR_MEMORY;
    }

    memset(new_rwlock, 0, sizeof(ppdb_base_rwlock_t));
    if (pthread_rwlock_init(&new_rwlock->rwlock, NULL) != 0) {
        ppdb_base_aligned_free(new_rwlock);
        return PPDB_BASE_ERR_RWLOCK;
    }

    new_rwlock->initialized = true;
    *rwlock = new_rwlock;
    return PPDB_OK;
}

void ppdb_base_rwlock_destroy(ppdb_base_rwlock_t* rwlock) {
    if (!rwlock) return;

    if (rwlock->initialized) {
        pthread_rwlock_destroy(&rwlock->rwlock);
    }
    ppdb_base_aligned_free(rwlock);
}

ppdb_error_t ppdb_base_rwlock_read_lock(ppdb_base_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) {
        return PPDB_BASE_ERR_PARAM;
    }

    if (pthread_rwlock_rdlock(&rwlock->rwlock) != 0) {
        return PPDB_BASE_ERR_RWLOCK;
    }

    rwlock->readers++;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_rwlock_read_unlock(ppdb_base_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) {
        return PPDB_BASE_ERR_PARAM;
    }

    if (pthread_rwlock_unlock(&rwlock->rwlock) != 0) {
        return PPDB_BASE_ERR_RWLOCK;
    }

    if (rwlock->readers > 0) {
        rwlock->readers--;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_base_rwlock_write_lock(ppdb_base_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) {
        return PPDB_BASE_ERR_PARAM;
    }

    if (pthread_rwlock_wrlock(&rwlock->rwlock) != 0) {
        return PPDB_BASE_ERR_RWLOCK;
    }

    rwlock->writers++;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_rwlock_write_unlock(ppdb_base_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) {
        return PPDB_BASE_ERR_PARAM;
    }

    if (pthread_rwlock_unlock(&rwlock->rwlock) != 0) {
        return PPDB_BASE_ERR_RWLOCK;
    }

    if (rwlock->writers > 0) {
        rwlock->writers--;
    }
    return PPDB_OK;
} 