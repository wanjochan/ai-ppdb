/*
 * base_counter.inc.c - Counter Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

// Create counter
ppdb_error_t ppdb_base_counter_create(ppdb_base_counter_t** counter) {
    if (!counter) {
        return PPDB_BASE_ERR_PARAM;
    }

    ppdb_base_counter_t* new_counter = (ppdb_base_counter_t*)malloc(sizeof(ppdb_base_counter_t));
    if (!new_counter) {
        return PPDB_BASE_ERR_MEMORY;
    }

    memset(new_counter, 0, sizeof(ppdb_base_counter_t));

    ppdb_error_t err = ppdb_base_mutex_create(&new_counter->mutex);
    if (err != PPDB_OK) {
        free(new_counter);
        return err;
    }

    *counter = new_counter;
    return PPDB_OK;
}

// Destroy counter
ppdb_error_t ppdb_base_counter_destroy(ppdb_base_counter_t* counter) {
    if (!counter) {
        return PPDB_BASE_ERR_PARAM;
    }

    if (counter->mutex) {
        ppdb_base_mutex_destroy(counter->mutex);
    }

    free(counter);
    return PPDB_OK;
}

// Get counter value
int64_t ppdb_base_counter_get(ppdb_base_counter_t* counter) {
    if (!counter) {
        return 0;
    }

    ppdb_base_mutex_lock(counter->mutex);
    int64_t value = counter->value;
    ppdb_base_mutex_unlock(counter->mutex);

    return value;
}

// Set counter value
void ppdb_base_counter_set(ppdb_base_counter_t* counter, int64_t value) {
    if (!counter) {
        return;
    }

    ppdb_base_mutex_lock(counter->mutex);
    counter->value = value;
    ppdb_base_mutex_unlock(counter->mutex);
}

// Increment counter
void ppdb_base_counter_inc(ppdb_base_counter_t* counter) {
    if (!counter) {
        return;
    }

    ppdb_base_mutex_lock(counter->mutex);
    counter->value++;
    ppdb_base_mutex_unlock(counter->mutex);
}

// Decrement counter
void ppdb_base_counter_dec(ppdb_base_counter_t* counter) {
    if (!counter) {
        return;
    }

    ppdb_base_mutex_lock(counter->mutex);
    if (counter->value > 0) {
        counter->value--;
    }
    ppdb_base_mutex_unlock(counter->mutex);
}

// Add value to counter
void ppdb_base_counter_add(ppdb_base_counter_t* counter, int64_t value) {
    if (!counter) {
        return;
    }

    ppdb_base_mutex_lock(counter->mutex);
    counter->value += value;
    ppdb_base_mutex_unlock(counter->mutex);
}

// Subtract value from counter
void ppdb_base_counter_sub(ppdb_base_counter_t* counter, int64_t value) {
    if (!counter) {
        return;
    }

    ppdb_base_mutex_lock(counter->mutex);
    counter->value -= value;
    ppdb_base_mutex_unlock(counter->mutex);
}

// Compare and exchange counter value
bool ppdb_base_counter_compare_exchange(ppdb_base_counter_t* counter, int64_t expected, int64_t desired) {
    if (!counter) {
        return false;
    }

    bool success = false;
    ppdb_base_mutex_lock(counter->mutex);
    if (counter->value == expected) {
        counter->value = desired;
        success = true;
    }
    ppdb_base_mutex_unlock(counter->mutex);

    return success;
}

// Reset counter
void ppdb_base_counter_reset(ppdb_base_counter_t* counter) {
    if (!counter) {
        return;
    }

    ppdb_base_mutex_lock(counter->mutex);
    counter->value = 0;
    ppdb_base_mutex_unlock(counter->mutex);
} 