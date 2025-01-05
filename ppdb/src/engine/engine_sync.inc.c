//-----------------------------------------------------------------------------
// Synchronization Primitives Implementation
//-----------------------------------------------------------------------------

struct ppdb_engine_mutex {
    pthread_mutex_t mutex;
    atomic_flag spinlock;
    atomic_uint64_t version;
    ppdb_engine_sync_config_t config;
    ppdb_engine_sync_stats_t* stats;
};

struct ppdb_engine_rwlock {
    union {
        pthread_rwlock_t rwlock;
        struct {
            atomic_uint32_t readers;
            atomic_flag writer;
            atomic_uint32_t writer_intent;
            atomic_uint64_t version;
        } lockfree;
    };
    ppdb_engine_sync_config_t config;
    ppdb_engine_sync_stats_t* stats;
};

struct ppdb_engine_cond {
    pthread_cond_t cond;
};

struct ppdb_engine_sync {
    pthread_mutex_t mutex;
    atomic_flag spinlock;
    atomic_uint64_t version;
    ppdb_engine_sync_config_t config;
    ppdb_engine_sync_stats_t* stats;
};

// Sync operations
ppdb_error_t ppdb_engine_sync_create(ppdb_engine_sync_t** sync, ppdb_engine_sync_config_t* config) {
    if (!sync || !config) return PPDB_ERR_NULL_POINTER;
    
    *sync = ppdb_engine_aligned_alloc(PPDB_ALIGNMENT, sizeof(ppdb_engine_sync_t));
    if (!*sync) return PPDB_ERR_OUT_OF_MEMORY;
    
    memset(*sync, 0, sizeof(ppdb_engine_sync_t));
    (*sync)->config = *config;
    
    // 初始化统计信息
    if (config->collect_stats) {
        (*sync)->stats = ppdb_engine_aligned_alloc(PPDB_ALIGNMENT, sizeof(ppdb_engine_sync_stats_t));
        if (!(*sync)->stats) {
            ppdb_engine_free(*sync);
            return PPDB_ERR_OUT_OF_MEMORY;
        }
        memset((*sync)->stats, 0, sizeof(ppdb_engine_sync_stats_t));
    }
    
    // 如果启用无锁模式，强制使用自旋锁
    if (config->use_lockfree) {
        atomic_flag_clear(&(*sync)->spinlock);
        atomic_store(&(*sync)->version, 0);
        return PPDB_OK;
    }
    
    // 否则使用pthread锁
    if (pthread_mutex_init(&(*sync)->mutex, NULL) != 0) {
        if ((*sync)->stats) ppdb_engine_free((*sync)->stats);
        ppdb_engine_free(*sync);
        return PPDB_ERR_INTERNAL;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_sync_destroy(ppdb_engine_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    if (!sync->config.use_lockfree) {
        if (pthread_mutex_destroy(&sync->mutex) != 0) {
            return PPDB_ERR_INTERNAL;
        }
    }
    
    if (sync->stats) {
        ppdb_engine_free(sync->stats);
    }
    
    ppdb_engine_free(sync);
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_sync_lock(ppdb_engine_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    uint64_t start_time = 0;
    if (sync->stats) {
        start_time = nowl();
    }
    
    if (sync->config.use_lockfree) {
        uint32_t spins = 0;
        uint32_t backoff = sync->config.backoff_us;
        
        while (atomic_flag_test_and_set(&sync->spinlock)) {
            if (++spins > sync->config.max_retries) {
                if (sync->stats) {
                    atomic_fetch_add(&sync->stats->timeout_count, 1);
                }
                return PPDB_ERR_TIMEOUT;
            }
            
            if (backoff > 0) {
                usleep(backoff);
                backoff = MIN(backoff * 2, sync->config.max_backoff_us);
            } else {
                sched_yield();
            }
            
            if (sync->stats) {
                atomic_fetch_add(&sync->stats->retry_count, 1);
            }
        }
        
        atomic_fetch_add(&sync->version, 1);
    } else {
        if (pthread_mutex_lock(&sync->mutex) != 0) {
            if (sync->stats) {
                atomic_fetch_add(&sync->stats->error_count, 1);
            }
            return PPDB_ERR_LOCK_FAILED;
        }
    }
    
    if (sync->stats && start_time > 0) {
        uint64_t wait_time = nowl() - start_time;
        atomic_fetch_add(&sync->stats->contention_count, 1);
        atomic_fetch_add(&sync->stats->total_wait_time_us, wait_time);
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_sync_unlock(ppdb_engine_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    if (sync->config.use_lockfree) {
        atomic_flag_clear(&sync->spinlock);
    } else {
        if (pthread_mutex_unlock(&sync->mutex) != 0) {
            if (sync->stats) {
                atomic_fetch_add(&sync->stats->error_count, 1);
            }
            return PPDB_ERR_UNLOCK_FAILED;
        }
    }
    
    return PPDB_OK;
}

// Mutex operations
ppdb_error_t ppdb_engine_mutex_create(ppdb_engine_mutex_t** mutex) {
    if (!mutex) return PPDB_ERR_NULL_POINTER;
    
    *mutex = ppdb_engine_aligned_alloc(PPDB_ALIGNMENT, sizeof(ppdb_engine_mutex_t));
    if (!*mutex) return PPDB_ERR_OUT_OF_MEMORY;
    
    memset(*mutex, 0, sizeof(ppdb_engine_mutex_t));
    atomic_flag_clear(&(*mutex)->spinlock);
    atomic_init(&(*mutex)->version, 0);
    
    (*mutex)->config.type = PPDB_ENGINE_SYNC_MUTEX;
    (*mutex)->config.use_lockfree = false;
    (*mutex)->config.spin_count = 1000;
    (*mutex)->config.timeout_ms = 0;
    (*mutex)->stats = NULL;
    
    if (pthread_mutex_init(&(*mutex)->mutex, NULL) != 0) {
        ppdb_engine_free(*mutex);
        *mutex = NULL;
        return PPDB_ERR_INTERNAL;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_mutex_destroy(ppdb_engine_mutex_t* mutex) {
    if (!mutex) return PPDB_ERR_NULL_POINTER;
    
    if (!mutex->config.use_lockfree) {
        if (pthread_mutex_destroy(&mutex->mutex) != 0) {
            return PPDB_ERR_INTERNAL;
        }
    }
    
    ppdb_engine_free(mutex);
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_mutex_lock(ppdb_engine_mutex_t* mutex) {
    if (!mutex) return PPDB_ERR_NULL_POINTER;
    
    if (mutex->config.use_lockfree) {
        uint32_t spins = 0;
        uint64_t start_time = 0;
        
        if (mutex->stats) {
            start_time = nowl();
        }
        
        while (atomic_flag_test_and_set(&mutex->spinlock)) {
            if (++spins >= mutex->config.spin_count) {
                usleep(MIN(spins * mutex->config.backoff_us, 
                          mutex->config.max_backoff_us));
                if (spins >= mutex->config.max_retries) {
                    return PPDB_ERR_TIMEOUT;
                }
            }
        }
        
        if (mutex->stats && start_time > 0) {
            uint64_t wait_time = nowl() - start_time;
            atomic_fetch_add(&mutex->stats->contention_count, 1);
            atomic_fetch_add(&mutex->stats->total_wait_time_us, wait_time);
            
            uint64_t current_max = atomic_load(&mutex->stats->max_wait_time_us);
            while (wait_time > current_max) {
                if (atomic_compare_exchange_weak(
                    &mutex->stats->max_wait_time_us,
                    &current_max,
                    wait_time)) {
                    break;
                }
            }
        }
        
        atomic_fetch_add(&mutex->version, 1);
        return PPDB_OK;
    }
    
    if (pthread_mutex_lock(&mutex->mutex) != 0) {
        return PPDB_ERR_LOCK_FAILED;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_mutex_unlock(ppdb_engine_mutex_t* mutex) {
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

ppdb_error_t ppdb_engine_mutex_trylock(ppdb_engine_mutex_t* mutex) {
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
ppdb_error_t ppdb_engine_rwlock_create(ppdb_engine_rwlock_t** lock) {
    if (!lock) return PPDB_ERR_NULL_POINTER;
    
    *lock = ppdb_engine_aligned_alloc(PPDB_ALIGNMENT, sizeof(ppdb_engine_rwlock_t));
    if (!*lock) return PPDB_ERR_OUT_OF_MEMORY;
    
    memset(*lock, 0, sizeof(ppdb_engine_rwlock_t));
    atomic_init(&(*lock)->lockfree.version, 0);
    
    (*lock)->config.type = PPDB_ENGINE_SYNC_RWLOCK;
    (*lock)->config.use_lockfree = false;
    (*lock)->config.spin_count = 1000;
    (*lock)->config.timeout_ms = 0;
    (*lock)->stats = NULL;
    
    if (!(*lock)->config.use_lockfree) {
        if (pthread_rwlock_init(&(*lock)->rwlock, NULL) != 0) {
            ppdb_engine_free(*lock);
            *lock = NULL;
            return PPDB_ERR_INTERNAL;
        }
    } else {
        atomic_store(&(*lock)->lockfree.readers, 0);
        atomic_flag_clear(&(*lock)->lockfree.writer);
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_rwlock_destroy(ppdb_engine_rwlock_t* lock) {
    if (!lock) return PPDB_ERR_NULL_POINTER;
    
    if (!lock->config.use_lockfree) {
        if (pthread_rwlock_destroy(&lock->rwlock) != 0) {
            return PPDB_ERR_INTERNAL;
        }
    }
    
    ppdb_engine_free(lock);
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_rwlock_rdlock(ppdb_engine_rwlock_t* lock) {
    if (!lock) return PPDB_ERR_NULL_POINTER;
    
    if (lock->config.use_lockfree) {
        uint32_t spins = 0;
        uint64_t start_time = 0;
        uint64_t start_version = atomic_load(&lock->lockfree.version);
        
        if (lock->stats) {
            start_time = nowl();
        }
        
        while (1) {
            // 检查是否有写意图
            if (atomic_load(&lock->lockfree.writer_intent) || 
                atomic_flag_test(&lock->lockfree.writer)) {
                if (++spins >= lock->config.spin_count) {
                    usleep(MIN(spins * lock->config.backoff_us,
                              lock->config.max_backoff_us));
                    if (spins >= lock->config.max_retries) {
                        return PPDB_ERR_TIMEOUT;
                    }
                }
                continue;
            }
            
            // 增加读者计数
            atomic_fetch_add(&lock->lockfree.readers, 1);
            
            // 再次检查写锁状态和版本号
            if (atomic_load(&lock->lockfree.version) == start_version &&
                !atomic_load(&lock->lockfree.writer_intent) &&
                !atomic_flag_test(&lock->lockfree.writer)) {
                break;
            }
            
            // 发现写锁，回退读者计数
            atomic_fetch_sub(&lock->lockfree.readers, 1);
            
            if (++spins >= lock->config.spin_count) {
                usleep(MIN(spins * lock->config.backoff_us,
                          lock->config.max_backoff_us));
                if (spins >= lock->config.max_retries) {
                    return PPDB_ERR_TIMEOUT;
                }
            }
        }
        
        if (lock->stats && start_time > 0) {
            uint64_t wait_time = nowl() - start_time;
            atomic_fetch_add(&lock->stats->contention_count, 1);
            atomic_fetch_add(&lock->stats->total_wait_time_us, wait_time);
            atomic_fetch_add(&lock->stats->concurrent_readers, 1);
        }
        
        return PPDB_OK;
    }
    
    if (pthread_rwlock_rdlock(&lock->rwlock) != 0) {
        return PPDB_ERR_LOCK_FAILED;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_rwlock_wrlock(ppdb_engine_rwlock_t* lock) {
    if (!lock) return PPDB_ERR_NULL_POINTER;
    
    if (lock->config.use_lockfree) {
        uint32_t spins = 0;
        uint64_t start_time = 0;
        
        if (lock->stats) {
            start_time = nowl();
        }

        // 设置写意图，防止新的读者进入
        atomic_store(&lock->lockfree.writer_intent, 1);
        
        // 尝试获取写锁
        while (atomic_flag_test_and_set(&lock->lockfree.writer)) {
            if (++spins >= lock->config.spin_count) {
                usleep(MIN(spins * lock->config.backoff_us,
                          lock->config.max_backoff_us));
                if (spins >= lock->config.max_retries) {
                    atomic_store(&lock->lockfree.writer_intent, 0);
                    return PPDB_ERR_TIMEOUT;
                }
            }
        }
        
        // 等待所有读者完成
        spins = 0;
        while (atomic_load(&lock->lockfree.readers) > 0) {
            if (++spins >= lock->config.spin_count) {
                usleep(MIN(spins * lock->config.backoff_us,
                          lock->config.max_backoff_us));
                if (spins >= lock->config.max_retries) {
                    atomic_flag_clear(&lock->lockfree.writer);
                    atomic_store(&lock->lockfree.writer_intent, 0);
                    return PPDB_ERR_TIMEOUT;
                }
            }
        }

        if (lock->stats && start_time > 0) {
            uint64_t wait_time = nowl() - start_time;
            atomic_fetch_add(&lock->stats->contention_count, 1);
            atomic_fetch_add(&lock->stats->total_wait_time_us, wait_time);
            atomic_fetch_add(&lock->stats->writer_queue_length, 1);
        }
        
        atomic_fetch_add(&lock->lockfree.version, 1);
        return PPDB_OK;
    }
    
    if (pthread_rwlock_wrlock(&lock->rwlock) != 0) {
        return PPDB_ERR_LOCK_FAILED;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_engine_rwlock_unlock(ppdb_engine_rwlock_t* lock) {
    if (!lock) return PPDB_ERR_NULL_POINTER;
    
    if (lock->config.use_lockfree) {
        if (atomic_load(&lock->lockfree.readers) > 0) {
            // 读锁解锁
            atomic_fetch_sub(&lock->lockfree.readers, 1);
            
            if (lock->stats) {
                atomic_fetch_sub(&lock->stats->concurrent_readers, 1);
            }
        } else {
            // 写锁解锁
            atomic_flag_clear(&lock->lockfree.writer);
            atomic_store(&lock->lockfree.writer_intent, 0);
            
            if (lock->stats) {
                atomic_fetch_sub(&lock->stats->writer_queue_length, 1);
            }
        }
        return PPDB_OK;
    }
    
    if (pthread_rwlock_unlock(&lock->rwlock) != 0) {
        return PPDB_ERR_UNLOCK_FAILED;
    }
    
    return PPDB_OK;
}

// Atomic operations
size_t ppdb_engine_atomic_load(const size_t* ptr) {
    return atomic_load((const atomic_size_t*)ptr);
}

void ppdb_engine_atomic_store(size_t* ptr, size_t val) {
    atomic_store((atomic_size_t*)ptr, val);
}

size_t ppdb_engine_atomic_add(size_t* ptr, size_t val) {
    return atomic_fetch_add((atomic_size_t*)ptr, val);
}

size_t ppdb_engine_atomic_sub(size_t* ptr, size_t val) {
    return atomic_fetch_sub((atomic_size_t*)ptr, val);
}

bool ppdb_engine_atomic_cas(size_t* ptr, size_t expected, size_t desired) {
    return atomic_compare_exchange_strong((atomic_size_t*)ptr, &expected, desired);
}
