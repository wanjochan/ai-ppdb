//-----------------------------------------------------------------------------
// 同步原语实现
//-----------------------------------------------------------------------------

// 创建同步对象
ppdb_error_t ppdb_sync_create(ppdb_sync_t** sync, ppdb_sync_config_t* config) {
    if (!sync || !config) return PPDB_ERR_NULL_POINTER;
    
    *sync = PPDB_ALIGNED_ALLOC(sizeof(ppdb_sync_t));
    if (!*sync) return PPDB_ERR_OUT_OF_MEMORY;
    memset(*sync, 0, sizeof(ppdb_sync_t));
    
    ppdb_error_t err = ppdb_sync_init(*sync, config);
    if (err != PPDB_OK) {
        PPDB_ALIGNED_FREE(*sync);
        *sync = NULL;
    }
    return err;
}

// 初始化同步对象
ppdb_error_t ppdb_sync_init(ppdb_sync_t* sync, ppdb_sync_config_t* config) {
    if (!sync || !config) return PPDB_ERR_NULL_POINTER;
    
    // 复制配置
    sync->config = *config;
    
    // 初始化统计信息
    ppdb_sync_counter_init(&sync->stats.read_locks, 0);
    ppdb_sync_counter_init(&sync->stats.write_locks, 0);
    ppdb_sync_counter_init(&sync->stats.read_timeouts, 0);
    ppdb_sync_counter_init(&sync->stats.write_timeouts, 0);
    ppdb_sync_counter_init(&sync->stats.retries, 0);
    
    // 根据类型初始化锁
    switch (config->type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_init(&sync->mutex, NULL) != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK:
            atomic_flag_clear(&sync->spinlock);
            break;
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_init(&sync->rwlock, NULL) != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            break;
            
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
    
    return PPDB_OK;
}

// 销毁同步对象
ppdb_error_t ppdb_sync_destroy(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    // 销毁统计信息
    ppdb_sync_counter_destroy(&sync->stats.read_locks);
    ppdb_sync_counter_destroy(&sync->stats.write_locks);
    ppdb_sync_counter_destroy(&sync->stats.read_timeouts);
    ppdb_sync_counter_destroy(&sync->stats.write_timeouts);
    ppdb_sync_counter_destroy(&sync->stats.retries);
    
    // 根据类型销毁锁
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            pthread_mutex_destroy(&sync->mutex);
            break;
            
        case PPDB_SYNC_SPINLOCK:
            // 自旋锁不需要销毁
            break;
            
        case PPDB_SYNC_RWLOCK:
            pthread_rwlock_destroy(&sync->rwlock);
            break;
    }
    
    return PPDB_OK;
}

// 加锁
ppdb_error_t ppdb_sync_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_lock(&sync->mutex) != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK: {
            uint32_t retries = 0;
            while (atomic_flag_test_and_set(&sync->spinlock)) {
                if (++retries > sync->config.max_retries) {
                    ppdb_sync_counter_add(&sync->stats.write_timeouts, 1);
                    return PPDB_ERR_TIMEOUT;
                }
                if (sync->config.backoff_us > 0) {
                    usleep(sync->config.backoff_us);
                }
                ppdb_sync_counter_add(&sync->stats.retries, 1);
            }
            break;
        }
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_wrlock(&sync->rwlock) != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            break;
            
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
    
    ppdb_sync_counter_add(&sync->stats.write_locks, 1);
    return PPDB_OK;
}

// 尝试加锁
ppdb_error_t ppdb_sync_try_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_trylock(&sync->mutex) != 0) {
                return PPDB_ERR_BUSY;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK:
            if (atomic_flag_test_and_set(&sync->spinlock)) {
                return PPDB_ERR_BUSY;
            }
            break;
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_trywrlock(&sync->rwlock) != 0) {
                return PPDB_ERR_BUSY;
            }
            break;
            
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
    
    ppdb_sync_counter_add(&sync->stats.write_locks, 1);
    return PPDB_OK;
}

// 解锁
ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_unlock(&sync->mutex) != 0) {
                return PPDB_ERR_UNLOCK_FAILED;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK:
            atomic_flag_clear(&sync->spinlock);
            break;
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_unlock(&sync->rwlock) != 0) {
                return PPDB_ERR_UNLOCK_FAILED;
            }
            break;
            
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
    
    return PPDB_OK;
}

// 读锁
ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_lock(&sync->mutex) != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK: {
            uint32_t retries = 0;
            while (atomic_flag_test_and_set(&sync->spinlock)) {
                if (++retries > sync->config.max_retries) {
                    ppdb_sync_counter_add(&sync->stats.read_timeouts, 1);
                    return PPDB_ERR_TIMEOUT;
                }
                if (sync->config.backoff_us > 0) {
                    usleep(sync->config.backoff_us);
                }
                ppdb_sync_counter_add(&sync->stats.retries, 1);
            }
            break;
        }
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_rdlock(&sync->rwlock) != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            break;
            
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
    
    ppdb_sync_counter_add(&sync->stats.read_locks, 1);
    return PPDB_OK;
}

// 写锁
ppdb_error_t ppdb_sync_write_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_lock(&sync->mutex) != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK: {
            uint32_t retries = 0;
            while (atomic_flag_test_and_set(&sync->spinlock)) {
                if (++retries > sync->config.max_retries) {
                    ppdb_sync_counter_add(&sync->stats.write_timeouts, 1);
                    return PPDB_ERR_TIMEOUT;
                }
                if (sync->config.backoff_us > 0) {
                    usleep(sync->config.backoff_us);
                }
                ppdb_sync_counter_add(&sync->stats.retries, 1);
            }
            break;
        }
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_wrlock(&sync->rwlock) != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            break;
            
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
    
    ppdb_sync_counter_add(&sync->stats.write_locks, 1);
    return PPDB_OK;
}

// 读解锁
ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_unlock(&sync->mutex) != 0) {
                return PPDB_ERR_UNLOCK_FAILED;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK:
            atomic_flag_clear(&sync->spinlock);
            break;
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_unlock(&sync->rwlock) != 0) {
                return PPDB_ERR_UNLOCK_FAILED;
            }
            break;
            
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
    
    return PPDB_OK;
}

// 写解锁
ppdb_error_t ppdb_sync_write_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_unlock(&sync->mutex) != 0) {
                return PPDB_ERR_UNLOCK_FAILED;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK:
            atomic_flag_clear(&sync->spinlock);
            break;
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_unlock(&sync->rwlock) != 0) {
                return PPDB_ERR_UNLOCK_FAILED;
            }
            break;
            
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
    
    return PPDB_OK;
}

// 尝试获取读锁
ppdb_error_t ppdb_sync_try_read_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_trylock(&sync->mutex) != 0) {
                return PPDB_ERR_BUSY;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK:
            if (atomic_flag_test_and_set(&sync->spinlock)) {
                return PPDB_ERR_BUSY;
            }
            break;
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_tryrdlock(&sync->rwlock) != 0) {
                return PPDB_ERR_BUSY;
            }
            break;
            
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
    
    ppdb_sync_counter_add(&sync->stats.read_locks, 1);
    return PPDB_OK;
}

// 尝试获取写锁
ppdb_error_t ppdb_sync_try_write_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_trylock(&sync->mutex) != 0) {
                return PPDB_ERR_BUSY;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK:
            if (atomic_flag_test_and_set(&sync->spinlock)) {
                return PPDB_ERR_BUSY;
            }
            break;
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_trywrlock(&sync->rwlock) != 0) {
                return PPDB_ERR_BUSY;
            }
            break;
            
        default:
            return PPDB_ERR_INVALID_TYPE;
    }
    
    ppdb_sync_counter_add(&sync->stats.write_locks, 1);
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// 计数器操作实现
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_sync_counter_init(ppdb_sync_counter_t* counter, size_t initial_value) {
    if (!counter) return PPDB_ERR_NULL_POINTER;
    
    atomic_init(&counter->value, initial_value);
    counter->lock = NULL;  // 默认不使用锁
    
    #ifdef PPDB_ENABLE_METRICS
    atomic_init(&counter->add_count, 0);
    atomic_init(&counter->sub_count, 0);
    counter->local_add_count = 0;
    counter->local_sub_count = 0;
    #endif
    
    return PPDB_OK;
}

void ppdb_sync_counter_destroy(ppdb_sync_counter_t* counter) {
    if (!counter) return;
    
    if (counter->lock) {
        ppdb_sync_destroy(counter->lock);
        counter->lock = NULL;
    }
}

size_t ppdb_sync_counter_add(ppdb_sync_counter_t* counter, size_t delta) {
    if (!counter) return 0;
    
    size_t old_value;
    if (counter->lock) {
        ppdb_sync_write_lock(counter->lock);
        old_value = atomic_load(&counter->value);
        atomic_store(&counter->value, old_value + delta);
        ppdb_sync_write_unlock(counter->lock);
    } else {
        old_value = atomic_fetch_add(&counter->value, delta);
    }
    
    #ifdef PPDB_ENABLE_METRICS
    atomic_fetch_add(&counter->add_count, 1);
    counter->local_add_count++;
    #endif
    
    return old_value;
}

size_t ppdb_sync_counter_sub(ppdb_sync_counter_t* counter, size_t delta) {
    if (!counter) return 0;
    
    size_t old_value;
    if (counter->lock) {
        ppdb_sync_write_lock(counter->lock);
        old_value = atomic_load(&counter->value);
        atomic_store(&counter->value, old_value - delta);
        ppdb_sync_write_unlock(counter->lock);
    } else {
        old_value = atomic_fetch_sub(&counter->value, delta);
    }
    
    #ifdef PPDB_ENABLE_METRICS
    atomic_fetch_add(&counter->sub_count, 1);
    counter->local_sub_count++;
    #endif
    
    return old_value;
}

size_t ppdb_sync_counter_load(ppdb_sync_counter_t* counter) {
    if (!counter) return 0;
    return atomic_load(&counter->value);
}

void ppdb_sync_counter_store(ppdb_sync_counter_t* counter, size_t value) {
    if (!counter) return;
    atomic_store(&counter->value, value);
}

bool ppdb_sync_counter_cas(ppdb_sync_counter_t* counter, size_t expected, size_t desired) {
    if (!counter) return false;
    return atomic_compare_exchange_strong(&counter->value, &expected, desired);
}

void ppdb_sync_counter_inc(ppdb_sync_counter_t* counter) {
    if (!counter) return;
    ppdb_sync_counter_add(counter, 1);
}

void ppdb_sync_counter_dec(ppdb_sync_counter_t* counter) {
    if (!counter) return;
    ppdb_sync_counter_sub(counter, 1);
}

uint64_t ppdb_sync_counter_get(ppdb_sync_counter_t* counter) {
    if (!counter) return 0;
    return ppdb_sync_counter_load(counter);
}

void ppdb_sync_counter_cleanup(ppdb_sync_counter_t* counter) {
    if (!counter) return;
    if (counter->lock) {
        ppdb_sync_destroy(counter->lock);
        PPDB_ALIGNED_FREE(counter->lock);
        counter->lock = NULL;
    }
    counter->value = 0;
} 