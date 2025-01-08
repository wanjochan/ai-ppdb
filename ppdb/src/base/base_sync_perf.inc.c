/*
 * base_sync_perf.inc.c - Synchronization Performance Functions
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// RWLock structure
struct ppdb_base_rwlock_s {
    pthread_rwlock_t rwlock;
    bool initialized;
};

// Time functions
uint64_t ppdb_base_get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

ppdb_error_t ppdb_base_sleep_us(uint32_t microseconds) {
    struct timespec ts;
    ts.tv_sec = microseconds / 1000000;
    ts.tv_nsec = (microseconds % 1000000) * 1000;
    nanosleep(&ts, NULL);
    return PPDB_OK;
}

// RWLock functions
ppdb_error_t ppdb_base_rwlock_create(ppdb_base_rwlock_t** rwlock) {
    if (!rwlock) return PPDB_BASE_ERR_PARAM;
    
    ppdb_base_rwlock_t* new_rwlock = malloc(sizeof(ppdb_base_rwlock_t));
    if (!new_rwlock) return PPDB_BASE_ERR_MEMORY;
    
    if (pthread_rwlock_init(&new_rwlock->rwlock, NULL) != 0) {
        free(new_rwlock);
        return PPDB_BASE_ERR_SYSTEM;
    }
    
    new_rwlock->initialized = true;
    *rwlock = new_rwlock;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_rwlock_destroy(ppdb_base_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) return PPDB_BASE_ERR_PARAM;
    
    pthread_rwlock_destroy(&rwlock->rwlock);
    rwlock->initialized = false;
    free(rwlock);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_rwlock_rdlock(ppdb_base_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) return PPDB_BASE_ERR_PARAM;
    
    if (pthread_rwlock_rdlock(&rwlock->rwlock) != 0) {
        return PPDB_BASE_ERR_SYSTEM;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_base_rwlock_wrlock(ppdb_base_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) return PPDB_BASE_ERR_PARAM;
    
    if (pthread_rwlock_wrlock(&rwlock->rwlock) != 0) {
        return PPDB_BASE_ERR_SYSTEM;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_base_rwlock_unlock(ppdb_base_rwlock_t* rwlock) {
    if (!rwlock || !rwlock->initialized) return PPDB_BASE_ERR_PARAM;
    
    if (pthread_rwlock_unlock(&rwlock->rwlock) != 0) {
        return PPDB_BASE_ERR_SYSTEM;
    }
    return PPDB_OK;
} 