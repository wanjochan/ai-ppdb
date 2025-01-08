/*
 * base_counter.inc.c - Counter Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Create counter
ppdb_error_t ppdb_base_counter_create(ppdb_base_counter_t** counter, const char* name) {
    if (!counter || !name) return PPDB_BASE_ERR_PARAM;

    ppdb_base_counter_t* new_counter = (ppdb_base_counter_t*)malloc(sizeof(ppdb_base_counter_t));
    if (!new_counter) return PPDB_BASE_ERR_MEMORY;

    atomic_init(&new_counter->value, 0);
    new_counter->name = strdup(name);
    if (!new_counter->name) {
        free(new_counter);
        return PPDB_BASE_ERR_MEMORY;
    }
    new_counter->stats_enabled = false;

    *counter = new_counter;
    return PPDB_OK;
}

// Destroy counter
ppdb_error_t ppdb_base_counter_destroy(ppdb_base_counter_t* counter) {
    if (!counter) return PPDB_BASE_ERR_PARAM;
    free(counter->name);
    free(counter);
    return PPDB_OK;
}

// Get counter value
uint64_t ppdb_base_counter_get(ppdb_base_counter_t* counter) {
    if (!counter) return 0;
    return atomic_load(&counter->value);
}

// Set counter value
ppdb_error_t ppdb_base_counter_set(ppdb_base_counter_t* counter, uint64_t value) {
    if (!counter) return PPDB_BASE_ERR_PARAM;
    atomic_store(&counter->value, value);
    return PPDB_OK;
}

// Increment counter
ppdb_error_t ppdb_base_counter_increment(ppdb_base_counter_t* counter) {
    if (!counter) return PPDB_BASE_ERR_PARAM;
    atomic_fetch_add(&counter->value, 1);
    return PPDB_OK;
}

// Decrement counter
ppdb_error_t ppdb_base_counter_decrement(ppdb_base_counter_t* counter) {
    if (!counter) return PPDB_BASE_ERR_PARAM;
    atomic_fetch_sub(&counter->value, 1);
    return PPDB_OK;
}

// Add value to counter
ppdb_error_t ppdb_base_counter_add(ppdb_base_counter_t* counter, int64_t value) {
    if (!counter) return PPDB_BASE_ERR_PARAM;
    atomic_fetch_add(&counter->value, value);
    return PPDB_OK;
}

// Subtract value from counter
ppdb_error_t ppdb_base_counter_sub(ppdb_base_counter_t* counter, int64_t value) {
    if (!counter) return PPDB_BASE_ERR_PARAM;
    atomic_fetch_sub(&counter->value, value);
    return PPDB_OK;
}

// Compare and exchange counter value
ppdb_error_t ppdb_base_counter_compare_exchange(ppdb_base_counter_t* counter, int64_t expected, int64_t desired) {
    if (!counter) return PPDB_BASE_ERR_PARAM;
    uint64_t exp = expected;
    bool success = atomic_compare_exchange_strong(&counter->value, &exp, desired);
    return success ? PPDB_OK : PPDB_BASE_ERR_BUSY;
}

// Reset counter
ppdb_error_t ppdb_base_counter_reset(ppdb_base_counter_t* counter) {
    if (!counter) return PPDB_BASE_ERR_PARAM;
    atomic_store(&counter->value, 0);
    return PPDB_OK;
} 