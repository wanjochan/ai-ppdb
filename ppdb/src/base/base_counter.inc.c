/*
 * base_counter.inc.c - 基础层原子计数器实现
 */

ppdb_error_t ppdb_base_counter_create(ppdb_base_counter_t** counter) {
    ppdb_base_counter_t* new_counter;
    ppdb_error_t err;

    if (!counter) return PPDB_ERR_PARAM;

    new_counter = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_base_counter_t));
    if (!new_counter) return PPDB_ERR_MEMORY;

    new_counter->value = 0;
    err = ppdb_base_mutex_create(&new_counter->mutex);
    if (err != PPDB_OK) {
        ppdb_base_aligned_free(new_counter);
        return err;
    }

    *counter = new_counter;
    return PPDB_OK;
}

void ppdb_base_counter_destroy(ppdb_base_counter_t* counter) {
    if (!counter) return;
    
    if (counter->mutex) {
        ppdb_base_mutex_destroy(counter->mutex);
    }
    ppdb_base_aligned_free(counter);
}

uint64_t ppdb_base_counter_increment(ppdb_base_counter_t* counter) {
    uint64_t result;
    
    if (!counter) return 0;
    
    ppdb_base_mutex_lock(counter->mutex);
    result = ++counter->value;
    ppdb_base_mutex_unlock(counter->mutex);
    
    return result;
}

uint64_t ppdb_base_counter_decrement(ppdb_base_counter_t* counter) {
    uint64_t result;
    
    if (!counter) return 0;
    
    ppdb_base_mutex_lock(counter->mutex);
    result = --counter->value;
    ppdb_base_mutex_unlock(counter->mutex);
    
    return result;
}

uint64_t ppdb_base_counter_get(ppdb_base_counter_t* counter) {
    uint64_t result;
    
    if (!counter) return 0;
    
    ppdb_base_mutex_lock(counter->mutex);
    result = counter->value;
    ppdb_base_mutex_unlock(counter->mutex);
    
    return result;
}

void ppdb_base_counter_set(ppdb_base_counter_t* counter, uint64_t value) {
    if (!counter) return;
    
    ppdb_base_mutex_lock(counter->mutex);
    counter->value = value;
    ppdb_base_mutex_unlock(counter->mutex);
} 