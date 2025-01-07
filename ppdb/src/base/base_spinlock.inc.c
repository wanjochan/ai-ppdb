/*
 * base_spinlock.inc.c - Spinlock Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Create spinlock
ppdb_error_t ppdb_base_spinlock_create(ppdb_base_spinlock_t** spinlock) {
    if (!spinlock) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_spinlock_t* new_spinlock = (ppdb_base_spinlock_t*)malloc(sizeof(ppdb_base_spinlock_t));
    if (!new_spinlock) {
        return PPDB_BASE_ERR_MEMORY;
    }

    memset(new_spinlock, 0, sizeof(ppdb_base_spinlock_t));
    atomic_store_explicit(&new_spinlock->locked, false, memory_order_relaxed);
    new_spinlock->initialized = true;
    new_spinlock->stats_enabled = false;
    new_spinlock->contention_count = 0;

    *spinlock = new_spinlock;
    return PPDB_OK;
}

// Destroy spinlock
ppdb_error_t ppdb_base_spinlock_destroy(ppdb_base_spinlock_t* spinlock) {
    if (!spinlock || !spinlock->initialized) {
        return PPDB_BASE_ERR_PARAM;
    }

    spinlock->initialized = false;
    free(spinlock);
    return PPDB_OK;
}

// Lock spinlock
ppdb_error_t ppdb_base_spinlock_lock(ppdb_base_spinlock_t* spinlock) {
    if (!spinlock || !spinlock->initialized) {
        return PPDB_BASE_ERR_PARAM;
    }

    bool expected = false;
    while (!atomic_compare_exchange_weak_explicit(
        &spinlock->locked,
        &expected,
        true,
        memory_order_acquire,
        memory_order_relaxed)) {
        expected = false;
        if (spinlock->stats_enabled) {
            spinlock->contention_count++;
        }
        ppdb_base_yield();
    }

    return PPDB_OK;
}

// Unlock spinlock
ppdb_error_t ppdb_base_spinlock_unlock(ppdb_base_spinlock_t* spinlock) {
    if (!spinlock || !spinlock->initialized) {
        return PPDB_BASE_ERR_PARAM;
    }

    atomic_store_explicit(&spinlock->locked, false, memory_order_release);
    return PPDB_OK;
}

// Try to lock spinlock
ppdb_error_t ppdb_base_spinlock_trylock(ppdb_base_spinlock_t* spinlock) {
    if (!spinlock || !spinlock->initialized) {
        return PPDB_BASE_ERR_PARAM;
    }

    bool expected = false;
    if (!atomic_compare_exchange_strong_explicit(
        &spinlock->locked,
        &expected,
        true,
        memory_order_acquire,
        memory_order_relaxed)) {
        return PPDB_BASE_ERR_BUSY;
    }

    return PPDB_OK;
}

// Enable/disable statistics
void ppdb_base_spinlock_enable_stats(ppdb_base_spinlock_t* spinlock, bool enable) {
    if (!spinlock || !spinlock->initialized) {
        return;
    }

    spinlock->stats_enabled = enable;
    if (enable) {
        spinlock->contention_count = 0;
    }
} 