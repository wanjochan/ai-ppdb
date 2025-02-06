#include "libc/thread/thread.h"
#include "libc/thread/semaphore.h"
#include "libc/atomic.h"
#include "InfraxSync.h"
#include "InfraxMemory.h"
#include "InfraxCore.h"

// Helper function for memory management
static InfraxMemory* get_memory_manager(void) {
    InfraxMemoryConfig mem_config = {
        .initial_size = 64 * 1024,
        .use_gc = true,
        .use_pool = true,
        .gc_threshold = 32 * 1024
    };
    return InfraxMemory_CLASS.new(&mem_config);
}

//-----------------------------------------------------------------------------
// Mutex Implementation
//-----------------------------------------------------------------------------

static InfraxError mutex_lock(InfraxSync* self) {
    if (!self || !self->is_initialized) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized mutex"
        };
    }

    int result = pthread_mutex_lock(&self->native_handle.mutex);
    if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_LOCK_FAILED,
            .message = "Failed to lock mutex"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

static InfraxError mutex_try_lock(InfraxSync* self) {
    if (!self || !self->is_initialized) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized mutex"
        };
    }

    int result = pthread_mutex_trylock(&self->native_handle.mutex);
    if (result == EBUSY) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_WOULD_BLOCK,
            .message = "Mutex is locked"
        };
    } else if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_LOCK_FAILED,
            .message = "Failed to lock mutex"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

static InfraxError mutex_unlock(InfraxSync* self) {
    if (!self || !self->is_initialized) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized mutex"
        };
    }

    int result = pthread_mutex_unlock(&self->native_handle.mutex);
    if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_UNLOCK_FAILED,
            .message = "Failed to unlock mutex"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

//-----------------------------------------------------------------------------
// RWLock Implementation
//-----------------------------------------------------------------------------

static InfraxError rwlock_read_lock(InfraxSync* self) {
    if (!self || !self->is_initialized) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized rwlock"
        };
    }

    int result = pthread_rwlock_rdlock(&self->native_handle.rwlock);
    if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_LOCK_FAILED,
            .message = "Failed to acquire read lock"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

static InfraxError rwlock_try_read_lock(InfraxSync* self) {
    if (!self || !self->is_initialized) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized rwlock"
        };
    }

    int result = pthread_rwlock_tryrdlock(&self->native_handle.rwlock);
    if (result == EBUSY) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_WOULD_BLOCK,
            .message = "Read lock is held by another thread"
        };
    } else if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_LOCK_FAILED,
            .message = "Failed to acquire read lock"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

static InfraxError rwlock_read_unlock(InfraxSync* self) {
    if (!self || !self->is_initialized) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized rwlock"
        };
    }

    int result = pthread_rwlock_unlock(&self->native_handle.rwlock);
    if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_UNLOCK_FAILED,
            .message = "Failed to release read lock"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

static InfraxError rwlock_write_lock(InfraxSync* self) {
    if (!self || !self->is_initialized) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized rwlock"
        };
    }

    int result = pthread_rwlock_wrlock(&self->native_handle.rwlock);
    if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_LOCK_FAILED,
            .message = "Failed to acquire write lock"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

static InfraxError rwlock_try_write_lock(InfraxSync* self) {
    if (!self || !self->is_initialized) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized rwlock"
        };
    }

    int result = pthread_rwlock_trywrlock(&self->native_handle.rwlock);
    if (result == EBUSY) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_WOULD_BLOCK,
            .message = "Write lock is held by another thread"
        };
    } else if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_LOCK_FAILED,
            .message = "Failed to acquire write lock"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

static InfraxError rwlock_write_unlock(InfraxSync* self) {
    if (!self || !self->is_initialized) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized rwlock"
        };
    }

    int result = pthread_rwlock_unlock(&self->native_handle.rwlock);
    if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_UNLOCK_FAILED,
            .message = "Failed to release write lock"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

static InfraxError spinlock_lock(InfraxSync* self) {
    if (!self || !self->is_initialized) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized spinlock"
        };
    }

    int result = pthread_spin_lock(&self->native_handle.spinlock);
    if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_LOCK_FAILED,
            .message = "Failed to acquire spinlock"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

static InfraxError spinlock_try_lock(InfraxSync* self) {
    if (!self || !self->is_initialized) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized spinlock"
        };
    }

    int result = pthread_spin_trylock(&self->native_handle.spinlock);
    if (result == EBUSY) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_WOULD_BLOCK,
            .message = "Spinlock is held by another thread"
        };
    } else if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_LOCK_FAILED,
            .message = "Failed to acquire spinlock"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

static InfraxError spinlock_unlock(InfraxSync* self) {
    if (!self || !self->is_initialized) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized spinlock"
        };
    }

    int result = pthread_spin_unlock(&self->native_handle.spinlock);
    if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_UNLOCK_FAILED,
            .message = "Failed to release spinlock"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

//-----------------------------------------------------------------------------
// Semaphore Implementation
//-----------------------------------------------------------------------------

static InfraxError semaphore_wait(InfraxSync* self) {
    if (!self || !self->is_initialized) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized semaphore"
        };
    }

    int result = sem_wait(&self->native_handle.semaphore);
    if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_WAIT_FAILED,
            .message = "Failed to wait on semaphore"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

static InfraxError semaphore_try_wait(InfraxSync* self) {
    if (!self || !self->is_initialized) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized semaphore"
        };
    }

    int result = sem_trywait(&self->native_handle.semaphore);
    if (result == -1 && errno == EAGAIN) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_WOULD_BLOCK,
            .message = "Semaphore count is zero"
        };
    } else if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_WAIT_FAILED,
            .message = "Failed to try wait on semaphore"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

static InfraxError semaphore_post(InfraxSync* self) {
    if (!self || !self->is_initialized) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized semaphore"
        };
    }

    int result = sem_post(&self->native_handle.semaphore);
    if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_SIGNAL_FAILED,
            .message = "Failed to post to semaphore"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

static InfraxError semaphore_get_value(InfraxSync* self, int* value) {
    if (!self || !self->is_initialized || !value) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized semaphore"
        };
    }

    int result = sem_getvalue(&self->native_handle.semaphore, value);
    if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_WAIT_FAILED,
            .message = "Failed to get semaphore value"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

//-----------------------------------------------------------------------------
// Condition Variable Implementation
//-----------------------------------------------------------------------------

static InfraxError cond_wait(InfraxSync* self, InfraxSync* mutex) {
    if (!self || !self->is_initialized || !mutex || !mutex->is_initialized) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized condition variable"
        };
    }

    int result = pthread_cond_wait(&self->native_handle.cond, &mutex->native_handle.mutex);
    if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_WAIT_FAILED,
            .message = "Failed to wait on condition variable"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

static InfraxError cond_timedwait(InfraxSync* self, InfraxSync* mutex, InfraxTime timeout_ms) {
    if (!self || !self->is_initialized || !mutex || !mutex->is_initialized) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized condition variable"
        };
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000;
    }

    int result = pthread_cond_timedwait(&self->native_handle.cond, &mutex->native_handle.mutex, &ts);
    if (result == ETIMEDOUT) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_TIMEOUT,
            .message = "Timed out waiting on condition variable"
        };
    } else if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_WAIT_FAILED,
            .message = "Failed to wait on condition variable"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

static InfraxError cond_signal(InfraxSync* self) {
    if (!self || !self->is_initialized) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized condition variable"
        };
    }

    int result = pthread_cond_signal(&self->native_handle.cond);
    if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_SIGNAL_FAILED,
            .message = "Failed to signal condition variable"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

static InfraxError cond_broadcast(InfraxSync* self) {
    if (!self || !self->is_initialized) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_INVALID_ARGUMENT,
            .message = "Invalid argument or uninitialized condition variable"
        };
    }

    int result = pthread_cond_broadcast(&self->native_handle.cond);
    if (result != 0) {
        return (InfraxError) {
            .code = INFRAX_ERROR_SYNC_SIGNAL_FAILED,
            .message = "Failed to broadcast condition variable"
        };
    }

    return (InfraxError) {
        .code = INFRAX_ERROR_OK,
        .message = "Success"
    };
}

//-----------------------------------------------------------------------------
// Atomic Operations Implementation
//-----------------------------------------------------------------------------

static int64_t infrax_atomic_load(InfraxSync* self) {
    return atomic_load(&self->value);
}

static void infrax_atomic_store(InfraxSync* self, int64_t value) {
    atomic_store(&self->value, value);
}

static int64_t infrax_atomic_exchange(InfraxSync* self, int64_t value) {
    return atomic_exchange(&self->value, value);
}

static bool infrax_atomic_compare_exchange(InfraxSync* self, int64_t* expected, int64_t desired) {
    return atomic_compare_exchange_strong(&self->value, expected, desired);
}

static int64_t infrax_atomic_fetch_add(InfraxSync* self, int64_t value) {
    return atomic_fetch_add(&self->value, value);
}

static int64_t infrax_atomic_fetch_sub(InfraxSync* self, int64_t value) {
    return atomic_fetch_sub(&self->value, value);
}

static int64_t infrax_atomic_fetch_and(InfraxSync* self, int64_t value) {
    return atomic_fetch_and(&self->value, value);
}

static int64_t infrax_atomic_fetch_or(InfraxSync* self, int64_t value) {
    return atomic_fetch_or(&self->value, value);
}

static int64_t infrax_atomic_fetch_xor(InfraxSync* self, int64_t value) {
    return atomic_fetch_xor(&self->value, value);
}

//-----------------------------------------------------------------------------
// Constructor and Destructor Implementation
//-----------------------------------------------------------------------------

static InfraxSync* infrax_sync_new(InfraxSyncType type) {
    InfraxMemory* memory = get_memory_manager();
    if (!memory) {
        return NULL;
    }

    InfraxSync* self = memory->alloc(memory, sizeof(InfraxSync));
    if (!self) {
        InfraxMemory_CLASS.free(memory);
        return NULL;
    }

    self->type = type;
    int result = 0;

    switch (type) {
        case INFRAX_SYNC_TYPE_MUTEX:
            // Initialize mutex with recursive attribute
            pthread_mutexattr_t mutex_attr;
            pthread_mutexattr_init(&mutex_attr);
            pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE);
            result = pthread_mutex_init(&self->native_handle.mutex, &mutex_attr);
            pthread_mutexattr_destroy(&mutex_attr);
            break;

        case INFRAX_SYNC_TYPE_RWLOCK:
            result = pthread_rwlock_init(&self->native_handle.rwlock, NULL);
            break;

        case INFRAX_SYNC_TYPE_SPINLOCK:
            result = pthread_spin_init(&self->native_handle.spinlock, PTHREAD_PROCESS_PRIVATE);
            break;

        case INFRAX_SYNC_TYPE_SEMAPHORE:
            result = sem_init(&self->native_handle.semaphore, 0, 1);
            break;

        case INFRAX_SYNC_TYPE_CONDITION:
            result = pthread_cond_init(&self->native_handle.cond, NULL);
            break;

        case INFRAX_SYNC_TYPE_ATOMIC:
            result = 0;  // Atomic operations don't need initialization
            break;

        default:
            memory->dealloc(memory, self);
            InfraxMemory_CLASS.free(memory);
            return NULL;
    }

    if (result != 0) {
        memory->dealloc(memory, self);
        InfraxMemory_CLASS.free(memory);
        return NULL;
    }

    self->klass = &InfraxSync_CLASS;
    self->is_initialized = true;
    atomic_init(&self->value, 0);

    self->mutex_lock = mutex_lock;
    self->mutex_try_lock = mutex_try_lock;
    self->mutex_unlock = mutex_unlock;

    self->rwlock_read_lock = rwlock_read_lock;
    self->rwlock_try_read_lock = rwlock_try_read_lock;
    self->rwlock_read_unlock = rwlock_read_unlock;
    self->rwlock_write_lock = rwlock_write_lock;
    self->rwlock_try_write_lock = rwlock_try_write_lock;
    self->rwlock_write_unlock = rwlock_write_unlock;

    self->spinlock_lock = spinlock_lock;
    self->spinlock_try_lock = spinlock_try_lock;
    self->spinlock_unlock = spinlock_unlock;

    self->semaphore_wait = semaphore_wait;
    self->semaphore_try_wait = semaphore_try_wait;
    self->semaphore_post = semaphore_post;
    self->semaphore_get_value = semaphore_get_value;

    self->cond_wait = cond_wait;
    self->cond_timedwait = cond_timedwait;
    self->cond_signal = cond_signal;
    self->cond_broadcast = cond_broadcast;

    self->atomic_load = infrax_atomic_load;
    self->atomic_store = infrax_atomic_store;
    self->atomic_exchange = infrax_atomic_exchange;
    self->atomic_compare_exchange = infrax_atomic_compare_exchange;
    self->atomic_fetch_add = infrax_atomic_fetch_add;
    self->atomic_fetch_sub = infrax_atomic_fetch_sub;
    self->atomic_fetch_and = infrax_atomic_fetch_and;
    self->atomic_fetch_or = infrax_atomic_fetch_or;
    self->atomic_fetch_xor = infrax_atomic_fetch_xor;

    return self;
}

static void infrax_sync_free(InfraxSync* self) {
    if (!self) {
        return;
    }

    InfraxMemory* memory = get_memory_manager();
    if (!memory) {
        return;
    }

    if (self->is_initialized) {
        switch (self->type) {
            case INFRAX_SYNC_TYPE_MUTEX:
                pthread_mutex_destroy(&self->native_handle.mutex);
                break;

            case INFRAX_SYNC_TYPE_RWLOCK:
                pthread_rwlock_destroy(&self->native_handle.rwlock);
                break;

            case INFRAX_SYNC_TYPE_SPINLOCK:
                pthread_spin_destroy(&self->native_handle.spinlock);
                break;

            case INFRAX_SYNC_TYPE_SEMAPHORE:
                sem_destroy(&self->native_handle.semaphore);
                break;

            case INFRAX_SYNC_TYPE_CONDITION:
                pthread_cond_destroy(&self->native_handle.cond);
                break;
        }
    }

    memory->dealloc(memory, self);
    InfraxMemory_CLASS.free(memory);
}

//-----------------------------------------------------------------------------
// Class Instance
//-----------------------------------------------------------------------------

const InfraxSyncClass InfraxSync_CLASS = {
    .new = infrax_sync_new,
    .free = infrax_sync_free
};