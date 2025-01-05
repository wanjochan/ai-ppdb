//-----------------------------------------------------------------------------
// 同步原语实现
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_sync_create(ppdb_sync_t** sync, ppdb_sync_config_t* config) {
    if (!sync || !config) {
        return PPDB_ERR_NULL_POINTER;
    }

    *sync = PPDB_ALIGNED_ALLOC(sizeof(ppdb_sync_t));
    if (!*sync) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    memset(*sync, 0, sizeof(ppdb_sync_t));
    (*sync)->config = *config;

    // 初始化统计计数器
    ppdb_sync_counter_init(&(*sync)->stats.read_locks, 0);
    ppdb_sync_counter_init(&(*sync)->stats.write_locks, 0);
    ppdb_sync_counter_init(&(*sync)->stats.read_timeouts, 0);
    ppdb_sync_counter_init(&(*sync)->stats.write_timeouts, 0);
    ppdb_sync_counter_init(&(*sync)->stats.retries, 0);

    // 根据类型初始化锁
    switch (config->type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_init(&(*sync)->mutex, NULL) != 0) {
                PPDB_ALIGNED_FREE(*sync);
                return PPDB_ERR_INTERNAL;
            }
            break;
            
        case PPDB_SYNC_SPINLOCK:
            atomic_flag_clear(&(*sync)->spinlock);
            break;
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_init(&(*sync)->rwlock, NULL) != 0) {
                PPDB_ALIGNED_FREE(*sync);
                return PPDB_ERR_INTERNAL;
            }
            break;
            
        default:
            PPDB_ALIGNED_FREE(*sync);
            return PPDB_ERR_INVALID_TYPE;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_sync_destroy(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;

    // 清理统计计数器
    ppdb_sync_counter_cleanup(&sync->stats.read_locks);
    ppdb_sync_counter_cleanup(&sync->stats.write_locks);
    ppdb_sync_counter_cleanup(&sync->stats.read_timeouts);
    ppdb_sync_counter_cleanup(&sync->stats.write_timeouts);
    ppdb_sync_counter_cleanup(&sync->stats.retries);

    // 根据类型清理锁
    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX:
            if (pthread_mutex_destroy(&sync->mutex) != 0) {
                PPDB_ALIGNED_FREE(sync);
                return PPDB_ERR_INTERNAL;
            }
            break;
            
        case PPDB_SYNC_RWLOCK:
            if (pthread_rwlock_destroy(&sync->rwlock) != 0) {
                PPDB_ALIGNED_FREE(sync);
                return PPDB_ERR_INTERNAL;
            }
            break;
            
        default:
            break;
    }

    PPDB_ALIGNED_FREE(sync);
    return PPDB_OK;
}

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
                    ppdb_sync_counter_inc(&sync->stats.retries);
                    return PPDB_ERR_TIMEOUT;
                }
                if (sync->config.backoff_us > 0) {
                    usleep(sync->config.backoff_us);
                }
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

    return PPDB_OK;
}

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

ppdb_error_t ppdb_sync_read_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    if (sync->config.type != PPDB_SYNC_RWLOCK) {
        return ppdb_sync_lock(sync);
    }
    
    if (pthread_rwlock_rdlock(&sync->rwlock) != 0) {
        ppdb_sync_counter_inc(&sync->stats.read_timeouts);
        return PPDB_ERR_LOCK_FAILED;
    }
    
    ppdb_sync_counter_inc(&sync->stats.read_locks);
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_read_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    if (sync->config.type != PPDB_SYNC_RWLOCK) {
        return ppdb_sync_unlock(sync);
    }
    
    if (pthread_rwlock_unlock(&sync->rwlock) != 0) {
        return PPDB_ERR_UNLOCK_FAILED;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_write_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    if (sync->config.type != PPDB_SYNC_RWLOCK) {
        return ppdb_sync_lock(sync);
    }
    
    if (pthread_rwlock_wrlock(&sync->rwlock) != 0) {
        ppdb_sync_counter_inc(&sync->stats.write_timeouts);
        return PPDB_ERR_LOCK_FAILED;
    }
    
    ppdb_sync_counter_inc(&sync->stats.write_locks);
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_write_unlock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    if (sync->config.type != PPDB_SYNC_RWLOCK) {
        return ppdb_sync_unlock(sync);
    }
    
    if (pthread_rwlock_unlock(&sync->rwlock) != 0) {
        return PPDB_ERR_UNLOCK_FAILED;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_try_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;

    switch (sync->config.type) {
        case PPDB_SYNC_MUTEX: {
            int ret = pthread_mutex_trylock(&sync->mutex);
            if (ret == EBUSY) {
                return PPDB_ERR_BUSY;
            } else if (ret != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            break;
        }
            
        case PPDB_SYNC_SPINLOCK:
            if (atomic_flag_test_and_set(&sync->spinlock)) {
                return PPDB_ERR_BUSY;
            }
            break;
            
        case PPDB_SYNC_RWLOCK: {
            int ret = pthread_rwlock_trywrlock(&sync->rwlock);
            if (ret == EBUSY) {
                return PPDB_ERR_BUSY;
            } else if (ret != 0) {
                return PPDB_ERR_LOCK_FAILED;
            }
            break;
        }
            
        default:
            return PPDB_ERR_INVALID_TYPE;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_sync_try_read_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    if (sync->config.type != PPDB_SYNC_RWLOCK) {
        return ppdb_sync_try_lock(sync);
    }
    
    int ret = pthread_rwlock_tryrdlock(&sync->rwlock);
    if (ret == EBUSY) {
        ppdb_sync_counter_inc(&sync->stats.read_timeouts);
        return PPDB_ERR_BUSY;
    } else if (ret != 0) {
        return PPDB_ERR_LOCK_FAILED;
    }
    
    ppdb_sync_counter_inc(&sync->stats.read_locks);
    return PPDB_OK;
}

ppdb_error_t ppdb_sync_try_write_lock(ppdb_sync_t* sync) {
    if (!sync) return PPDB_ERR_NULL_POINTER;
    
    if (sync->config.type != PPDB_SYNC_RWLOCK) {
        return ppdb_sync_try_lock(sync);
    }
    
    int ret = pthread_rwlock_trywrlock(&sync->rwlock);
    if (ret == EBUSY) {
        ppdb_sync_counter_inc(&sync->stats.write_timeouts);
        return PPDB_ERR_BUSY;
    } else if (ret != 0) {
        return PPDB_ERR_LOCK_FAILED;
    }
    
    ppdb_sync_counter_inc(&sync->stats.write_locks);
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
    counter->local_sub_count++;
    #endif
    
    return old_value;
}

size_t ppdb_sync_counter_load(ppdb_sync_counter_t* counter) {
    if (!counter) return 0;
    return atomic_load(&counter->value);
}

size_t ppdb_sync_counter_store(ppdb_sync_counter_t* counter, size_t value) {
    if (!counter) return 0;
    size_t old_value = atomic_exchange(&counter->value, value);
    return old_value;
}

bool ppdb_sync_counter_cas(ppdb_sync_counter_t* counter, size_t expected, size_t desired) {
    if (!counter) return false;
    return atomic_compare_exchange_strong(&counter->value, &expected, desired);
}

size_t ppdb_sync_counter_inc(ppdb_sync_counter_t* counter) {
    if (!counter) return 0;
    return ppdb_sync_counter_add(counter, 1);
}

size_t ppdb_sync_counter_dec(ppdb_sync_counter_t* counter) {
    if (!counter) return 0;
    return ppdb_sync_counter_sub(counter, 1);
}

size_t ppdb_sync_counter_get(ppdb_sync_counter_t* counter) {
    if (!counter) return 0;
    return ppdb_sync_counter_load(counter);
}

size_t ppdb_sync_counter_cleanup(ppdb_sync_counter_t* counter) {
    if (!counter) return 0;
    if (counter->lock) {
        ppdb_sync_destroy(counter->lock);
        PPDB_ALIGNED_FREE(counter->lock);
        counter->lock = NULL;
    }
    size_t old_value = atomic_load(&counter->value);
    counter->value = 0;
    return old_value;
} 