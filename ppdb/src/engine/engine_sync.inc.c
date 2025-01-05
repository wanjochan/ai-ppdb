//-----------------------------------------------------------------------------
// Synchronization Primitives Implementation - Forwarding to Base Layer
//-----------------------------------------------------------------------------

// Forward sync operations to base layer
ppdb_error_t ppdb_base_sync_create(ppdb_base_sync_t** sync, ppdb_base_sync_config_t* config) {
    return ppdb_base_sync_create(sync, config);
}

ppdb_error_t ppdb_base_sync_destroy(ppdb_base_sync_t* sync) {
    return ppdb_base_sync_destroy(sync);
}

ppdb_error_t ppdb_base_sync_lock(ppdb_base_sync_t* sync) {
    return ppdb_base_sync_lock(sync);
}

ppdb_error_t ppdb_base_sync_unlock(ppdb_base_sync_t* sync) {
    return ppdb_base_sync_unlock(sync);
}

// Forward mutex operations to base layer
ppdb_error_t ppdb_base_mutex_create(ppdb_base_mutex_t** mutex) {
    return ppdb_base_mutex_create(mutex);
}

ppdb_error_t ppdb_base_mutex_destroy(ppdb_base_mutex_t* mutex) {
    return ppdb_base_mutex_destroy(mutex);
}

ppdb_error_t ppdb_base_mutex_lock(ppdb_base_mutex_t* mutex) {
    return ppdb_base_mutex_lock(mutex);
}

ppdb_error_t ppdb_base_mutex_unlock(ppdb_base_mutex_t* mutex) {
    return ppdb_base_mutex_unlock(mutex);
}

ppdb_error_t ppdb_base_mutex_trylock(ppdb_base_mutex_t* mutex) {
    return ppdb_base_mutex_trylock(mutex);
}

// Forward rwlock operations to base layer
ppdb_error_t ppdb_base_rwlock_create(ppdb_base_rwlock_t** lock) {
    return ppdb_base_rwlock_create(lock);
}

ppdb_error_t ppdb_base_rwlock_destroy(ppdb_base_rwlock_t* lock) {
    return ppdb_base_rwlock_destroy(lock);
}

ppdb_error_t ppdb_base_rwlock_rdlock(ppdb_base_rwlock_t* lock) {
    return ppdb_base_rwlock_rdlock(lock);
}

ppdb_error_t ppdb_base_rwlock_wrlock(ppdb_base_rwlock_t* lock) {
    return ppdb_base_rwlock_wrlock(lock);
}

ppdb_error_t ppdb_base_rwlock_unlock(ppdb_base_rwlock_t* lock) {
    return ppdb_base_rwlock_unlock(lock);
}

// Forward atomic operations to base layer
size_t ppdb_base_atomic_load(const size_t* ptr) {
    return ppdb_base_atomic_load(ptr);
}

void ppdb_base_atomic_store(size_t* ptr, size_t val) {
    ppdb_base_atomic_store(ptr, val);
}

size_t ppdb_base_atomic_add(size_t* ptr, size_t val) {
    return ppdb_base_atomic_add(ptr, val);
}

size_t ppdb_base_atomic_sub(size_t* ptr, size_t val) {
    return ppdb_base_atomic_sub(ptr, val);
}

bool ppdb_base_atomic_cas(size_t* ptr, size_t expected, size_t desired) {
    return ppdb_base_atomic_cas(ptr, expected, desired);
}
