/*
 * engine_mutex.inc.c - Engine Mutex Implementation
 */

// Mutex operations
ppdb_error_t ppdb_engine_mutex_create(ppdb_base_mutex_t** mutex) {
    return ppdb_base_mutex_create(mutex);
}

void ppdb_engine_mutex_destroy(ppdb_base_mutex_t* mutex) {
    ppdb_base_mutex_destroy(mutex);
}

ppdb_error_t ppdb_engine_mutex_lock(ppdb_base_mutex_t* mutex) {
    return ppdb_base_mutex_lock(mutex);
}

ppdb_error_t ppdb_engine_mutex_unlock(ppdb_base_mutex_t* mutex) {
    return ppdb_base_mutex_unlock(mutex);
} 