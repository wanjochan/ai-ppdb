//-----------------------------------------------------------------------------
// Synchronization Primitives Implementation
//-----------------------------------------------------------------------------

struct ppdb_core_mutex {
    pthread_mutex_t mutex;
    atomic_flag spinlock;
    ppdb_core_sync_config_t config;
};

struct ppdb_core_rwlock {
    union {
        pthread_rwlock_t rwlock;
        struct {
            atomic_size_t readers;
            atomic_flag writer;
        } lockfree;
    };
    ppdb_core_sync_config_t config;
};

struct ppdb_core_cond {
    pthread_cond_t cond;
};

// Mutex operations
ppdb_error_t ppdb_core_mutex_create(ppdb_core_mutex_t** mutex) {
    if (!mutex) return PPDB_ERR_NULL_POINTER;
    
    *mutex = ppdb_core_aligned_alloc(PPDB_ALIGNMENT, sizeof(ppdb_core_mutex_t));
    if (!*mutex) return PPDB_ERR_OUT_OF_MEMORY;
    
    memset(*mutex, 0, sizeof(ppdb_core_mutex_t));
    atomic_flag_clear(&(*mutex)->spinlock);
    
    (*mutex)->config.type = PPDB_CORE_SYNC_MUTEX;
    (*mutex)->config.use_lockfree = false;
    (*mutex)->config.spin_count = 1000;
    (*mutex)->config.timeout_ms = 0;
    
    if (pthread_mutex_init(&(*mutex)->mutex, NULL) != 0) {
        ppdb_core_free(*mutex);
        *mutex = NULL;
        return PPDB_ERR_INTERNAL;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_core_mutex_destroy(ppdb_core_mutex_t* mutex) {
    if (!mutex) return PPDB_ERR_NULL_POINTER;
    
    if (!mutex->config.use_lockfree) {
        if (pthread_mutex_destroy(&mutex->mutex) != 0) {
            return PPDB_ERR_INTERNAL;
        }
    }
    
    ppdb_core_free(mutex);
    return PPDB_OK;
}

ppdb_error_t ppdb_core_mutex_lock(ppdb_core_mutex_t* mutex) {
    if (!mutex) return PPDB_ERR_NULL_POINTER;
    
    if (mutex->config.use_lockfree) {
        // 自旋锁实现
        uint32_t spins = 0;
        while (atomic_flag_test_and_set(&mutex->spinlock)) {
            if (++spins >= mutex->config.spin_count) {
                ppdb_core_thread_yield();
                spins = 0;
            }
        }
        return PPDB_OK;
    }
    
    if (pthread_mutex_lock(&mutex->mutex) != 0) {
        return PPDB_ERR_LOCK_FAILED;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_core_mutex_unlock(ppdb_core_mutex_t* mutex) {
    if (!mutex) return PPDB_ERR_NULL_POINTER;
    
    if (mutex->config.use_lockfree) {
        atomic_flag_clear(&mutex->spinlock);
        return PPDB_OK;
    }
    
    if (pthread_mutex_unlock(&mutex->mutex) != 0) {
        return PPDB_ERR_UNLOCK_FAILED;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_core_mutex_trylock(ppdb_core_mutex_t* mutex) {
    if (!mutex) return PPDB_ERR_NULL_POINTER;
    
    if (mutex->config.use_lockfree) {
        if (atomic_flag_test_and_set(&mutex->spinlock)) {
            return PPDB_ERR_WOULD_BLOCK;
        }
        return PPDB_OK;
    }
    
    int ret = pthread_mutex_trylock(&mutex->mutex);
    if (ret == EBUSY) {
        return PPDB_ERR_WOULD_BLOCK;
    } else if (ret != 0) {
        return PPDB_ERR_LOCK_FAILED;
    }
    
    return PPDB_OK;
}

// RWLock operations
ppdb_error_t ppdb_core_rwlock_create(ppdb_core_rwlock_t** lock) {
    if (!lock) return PPDB_ERR_NULL_POINTER;
    
    *lock = ppdb_core_aligned_alloc(PPDB_ALIGNMENT, sizeof(ppdb_core_rwlock_t));
    if (!*lock) return PPDB_ERR_OUT_OF_MEMORY;
    
    memset(*lock, 0, sizeof(ppdb_core_rwlock_t));
    
    (*lock)->config.type = PPDB_CORE_SYNC_RWLOCK;
    (*lock)->config.use_lockfree = false;
    (*lock)->config.spin_count = 1000;
    (*lock)->config.timeout_ms = 0;
    
    if (!(*lock)->config.use_lockfree) {
        if (pthread_rwlock_init(&(*lock)->rwlock, NULL) != 0) {
            ppdb_core_free(*lock);
            *lock = NULL;
            return PPDB_ERR_INTERNAL;
        }
    } else {
        atomic_store(&(*lock)->lockfree.readers, 0);
        atomic_flag_clear(&(*lock)->lockfree.writer);
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_core_rwlock_destroy(ppdb_core_rwlock_t* lock) {
    if (!lock) return PPDB_ERR_NULL_POINTER;
    
    if (!lock->config.use_lockfree) {
        if (pthread_rwlock_destroy(&lock->rwlock) != 0) {
            return PPDB_ERR_INTERNAL;
        }
    }
    
    ppdb_core_free(lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_core_rwlock_rdlock(ppdb_core_rwlock_t* lock) {
    if (!lock) return PPDB_ERR_NULL_POINTER;
    
    if (lock->config.use_lockfree) {
        uint32_t spins = 0;
        while (1) {
            // 等待写锁释放
            while (atomic_flag_test(&lock->lockfree.writer)) {
                if (++spins >= lock->config.spin_count) {
                    ppdb_core_thread_yield();
                    spins = 0;
                }
            }
            
            // 增加读者计数
            atomic_fetch_add(&lock->lockfree.readers, 1);
            
            // 再次检查写锁
            if (!atomic_flag_test(&lock->lockfree.writer)) {
                break;
            }
            
            // 如果写锁被占用，回退
            atomic_fetch_sub(&lock->lockfree.readers, 1);
        }
        return PPDB_OK;
    }
    
    if (pthread_rwlock_rdlock(&lock->rwlock) != 0) {
        return PPDB_ERR_LOCK_FAILED;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_core_rwlock_wrlock(ppdb_core_rwlock_t* lock) {
    if (!lock) return PPDB_ERR_NULL_POINTER;
    
    if (lock->config.use_lockfree) {
        uint32_t spins = 0;
        
        // 尝试获取写锁
        while (atomic_flag_test_and_set(&lock->lockfree.writer)) {
            if (++spins >= lock->config.spin_count) {
                ppdb_core_thread_yield();
                spins = 0;
            }
        }
        
        // 等待所有读者完成
        spins = 0;
        while (atomic_load(&lock->lockfree.readers) > 0) {
            if (++spins >= lock->config.spin_count) {
                ppdb_core_thread_yield();
                spins = 0;
            }
        }
        
        return PPDB_OK;
    }
    
    if (pthread_rwlock_wrlock(&lock->rwlock) != 0) {
        return PPDB_ERR_LOCK_FAILED;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_core_rwlock_unlock(ppdb_core_rwlock_t* lock) {
    if (!lock) return PPDB_ERR_NULL_POINTER;
    
    if (lock->config.use_lockfree) {
        if (atomic_load(&lock->lockfree.readers) > 0) {
            // 读锁解锁
            atomic_fetch_sub(&lock->lockfree.readers, 1);
        } else {
            // 写锁解锁
            atomic_flag_clear(&lock->lockfree.writer);
        }
        return PPDB_OK;
    }
    
    if (pthread_rwlock_unlock(&lock->rwlock) != 0) {
        return PPDB_ERR_UNLOCK_FAILED;
    }
    
    return PPDB_OK;
}

// Atomic operations
size_t ppdb_core_atomic_load(const size_t* ptr) {
    return atomic_load((const atomic_size_t*)ptr);
}

void ppdb_core_atomic_store(size_t* ptr, size_t val) {
    atomic_store((atomic_size_t*)ptr, val);
}

size_t ppdb_core_atomic_add(size_t* ptr, size_t val) {
    return atomic_fetch_add((atomic_size_t*)ptr, val);
}

size_t ppdb_core_atomic_sub(size_t* ptr, size_t val) {
    return atomic_fetch_sub((atomic_size_t*)ptr, val);
}

bool ppdb_core_atomic_cas(size_t* ptr, size_t expected, size_t desired) {
    return atomic_compare_exchange_strong((atomic_size_t*)ptr, &expected, desired);
}
