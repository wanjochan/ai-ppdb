#include "InfraxSync.h"
#include "InfraxMemory.h"
#include "InfraxCore.h"

// Forward declaration of static variables
static bool is_initialized = false;
static pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;

// Forward declarations of instance methods
static InfraxError mutex_lock(InfraxSync* self);
static InfraxError mutex_try_lock(InfraxSync* self);
static InfraxError mutex_unlock(InfraxSync* self);
static InfraxError rwlock_read_lock(InfraxSync* self);
static InfraxError rwlock_try_read_lock(InfraxSync* self);
static InfraxError rwlock_read_unlock(InfraxSync* self);
static InfraxError rwlock_write_lock(InfraxSync* self);
static InfraxError rwlock_try_write_lock(InfraxSync* self);
static InfraxError rwlock_write_unlock(InfraxSync* self);
static InfraxError spinlock_lock(InfraxSync* self);
static InfraxError spinlock_try_lock(InfraxSync* self);
static InfraxError spinlock_unlock(InfraxSync* self);
static InfraxError semaphore_wait(InfraxSync* self);
static InfraxError semaphore_try_wait(InfraxSync* self);
static InfraxError semaphore_post(InfraxSync* self);
static InfraxError semaphore_get_value(InfraxSync* self, int* value);
static InfraxError cond_wait(InfraxSync* self, InfraxSync* mutex);
static InfraxError cond_timedwait(InfraxSync* self, InfraxSync* mutex, InfraxTime timeout_ms);
static InfraxError cond_signal(InfraxSync* self);
static InfraxError cond_broadcast(InfraxSync* self);
static int64_t infrax_atomic_load(InfraxSync* self);
static void infrax_atomic_store(InfraxSync* self, int64_t value);
static int64_t infrax_atomic_exchange(InfraxSync* self, int64_t value);
static bool infrax_atomic_compare_exchange(InfraxSync* self, int64_t* expected, int64_t desired);
static int64_t infrax_atomic_fetch_add(InfraxSync* self, int64_t value);
static int64_t infrax_atomic_fetch_sub(InfraxSync* self, int64_t value);
static int64_t infrax_atomic_fetch_and(InfraxSync* self, int64_t value);
static int64_t infrax_atomic_fetch_or(InfraxSync* self, int64_t value);
static int64_t infrax_atomic_fetch_xor(InfraxSync* self, int64_t value);

// 添加缺失的函数声明
static int64_t cond_exchange(InfraxSync* self, int64_t value);
static bool cond_compare_exchange(InfraxSync* self, int64_t* expected, int64_t desired);
static InfraxError cond_fetch_add(InfraxSync* self, int64_t value);
static InfraxError cond_fetch_sub(InfraxSync* self, int64_t value);
static InfraxError cond_fetch_and(InfraxSync* self, int64_t value);
static InfraxError cond_fetch_or(InfraxSync* self, int64_t value);
static InfraxError cond_fetch_xor(InfraxSync* self, int64_t value);

// 添加初始化函数声明
static InfraxError mutex_init(InfraxSync* self);
static InfraxError rwlock_init(InfraxSync* self);
static InfraxError spinlock_init(InfraxSync* self);
static InfraxError semaphore_init(InfraxSync* self);
static InfraxError condition_init(InfraxSync* self);
static InfraxError atomic_init_sync(InfraxSync* self);

// Helper function for memory management
static InfraxMemory* get_memory_manager(void) {
    static InfraxMemory* memory = NULL;
    if (!memory) {
        InfraxMemoryConfig config = {
            .initial_size = 1024 * 1024,  // 1MB
            .use_gc = false,
            .use_pool = true,
            .gc_threshold = 0
        };
        memory = InfraxMemoryClass.new(&config);
    }
    return memory;
}

// Private initialization function
static InfraxError infrax_sync_init(InfraxSync* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid sync object");
    }

    InfraxError err = INFRAX_ERROR_OK_STRUCT;

    switch (self->type) {
        case INFRAX_SYNC_TYPE_MUTEX:
            err = mutex_init(self);
            break;
        case INFRAX_SYNC_TYPE_RWLOCK:
            err = rwlock_init(self);
            break;
        case INFRAX_SYNC_TYPE_SPINLOCK:
            err = spinlock_init(self);
            break;
        case INFRAX_SYNC_TYPE_SEMAPHORE:
            err = semaphore_init(self);
            break;
        case INFRAX_SYNC_TYPE_CONDITION:
            err = condition_init(self);
            break;
        case INFRAX_SYNC_TYPE_ATOMIC:
            err = atomic_init_sync(self);
            break;
        default:
            return make_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid sync type");
    }

    if (err.code == 0) {
        self->is_initialized = true;
    }

    return err;
}

// Factory implementation
static InfraxSync* infrax_sync_new(InfraxSyncType type) {
    pthread_mutex_lock(&init_mutex);
    if (!is_initialized) {
        is_initialized = true;
    }
    pthread_mutex_unlock(&init_mutex);

    InfraxMemory* memory = get_memory_manager();
    if (!memory) return NULL;

    InfraxSync* sync = (InfraxSync*)memory->alloc(memory, sizeof(InfraxSync));
    if (!sync) return NULL;

    // Initialize the sync object
    InfraxError err = infrax_sync_init(sync);
    if (err.code != 0) {
        memory->dealloc(memory, sync);
        return NULL;
    }

    // Initialize specific sync primitive based on type
    switch (type) {
        case INFRAX_SYNC_TYPE_MUTEX:
            pthread_mutex_init(&sync->native_handle.mutex, NULL);
            break;
        case INFRAX_SYNC_TYPE_CONDITION:
            pthread_cond_init(&sync->native_handle.cond, NULL);
            break;
        case INFRAX_SYNC_TYPE_RWLOCK:
            pthread_rwlock_init(&sync->native_handle.rwlock, NULL);
            break;
        case INFRAX_SYNC_TYPE_SPINLOCK:
            pthread_spin_init(&sync->native_handle.spin, PTHREAD_PROCESS_PRIVATE);
            break;
        case INFRAX_SYNC_TYPE_SEMAPHORE:
            sem_init(&sync->native_handle.sem, 0, 0);
            break;
        case INFRAX_SYNC_TYPE_ATOMIC:
            // Nothing to clean up for atomic
            break;
        default:
            memory->dealloc(memory, sync);
            return NULL;
    }

    sync->type = type;
    sync->self = sync;
    sync->klass = &InfraxSyncClass;
    return sync;
}

// Free function implementation
static void infrax_sync_free(InfraxSync* sync) {
    if (!sync) return;

    // Clean up based on type
    switch (sync->type) {
        case INFRAX_SYNC_TYPE_MUTEX:
            pthread_mutex_destroy(&sync->native_handle.mutex);
            break;
        case INFRAX_SYNC_TYPE_CONDITION:
            pthread_cond_destroy(&sync->native_handle.cond);
            break;
        case INFRAX_SYNC_TYPE_RWLOCK:
            pthread_rwlock_destroy(&sync->native_handle.rwlock);
            break;
        case INFRAX_SYNC_TYPE_SPINLOCK:
            pthread_spin_destroy(&sync->native_handle.spin);
            break;
        case INFRAX_SYNC_TYPE_SEMAPHORE:
            sem_destroy(&sync->native_handle.sem);
            break;
        case INFRAX_SYNC_TYPE_ATOMIC:
            // Nothing to clean up for atomic
            break;
    }

    // Free the memory
    InfraxMemory* memory = get_memory_manager();
    if (memory) {
        memory->dealloc(memory, sync);
    }
}

// The "static" interface implementation
InfraxSyncClassType InfraxSyncClass = {
    .new = infrax_sync_new,
    .free = infrax_sync_free,
    .mutex_lock = mutex_lock,
    .mutex_try_lock = mutex_try_lock,
    .mutex_unlock = mutex_unlock,
    .rwlock_read_lock = rwlock_read_lock,
    .rwlock_try_read_lock = rwlock_try_read_lock,
    .rwlock_read_unlock = rwlock_read_unlock,
    .rwlock_write_lock = rwlock_write_lock,
    .rwlock_try_write_lock = rwlock_try_write_lock,
    .rwlock_write_unlock = rwlock_write_unlock,
    .spinlock_lock = spinlock_lock,
    .spinlock_try_lock = spinlock_try_lock,
    .spinlock_unlock = spinlock_unlock,
    .semaphore_wait = semaphore_wait,
    .semaphore_try_wait = semaphore_try_wait,
    .semaphore_post = semaphore_post,
    .semaphore_get_value = semaphore_get_value,
    .cond_wait = cond_wait,
    .cond_timedwait = cond_timedwait,
    .cond_signal = cond_signal,
    .cond_broadcast = cond_broadcast,
    .cond_exchange = cond_exchange,
    .cond_compare_exchange = cond_compare_exchange,
    .cond_fetch_add = cond_fetch_add,
    .cond_fetch_sub = cond_fetch_sub,
    .cond_fetch_and = cond_fetch_and,
    .cond_fetch_or = cond_fetch_or,
    .cond_fetch_xor = cond_fetch_xor,
    .atomic_load = infrax_atomic_load,
    .atomic_store = infrax_atomic_store,
    .atomic_exchange = infrax_atomic_exchange,
    .atomic_compare_exchange = infrax_atomic_compare_exchange,
    .atomic_fetch_add = infrax_atomic_fetch_add,
    .atomic_fetch_sub = infrax_atomic_fetch_sub,
    .atomic_fetch_and = infrax_atomic_fetch_and,
    .atomic_fetch_or = infrax_atomic_fetch_or,
    .atomic_fetch_xor = infrax_atomic_fetch_xor
};

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

    int result = pthread_spin_lock(&self->native_handle.spin);
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

    int result = pthread_spin_trylock(&self->native_handle.spin);
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

    int result = pthread_spin_unlock(&self->native_handle.spin);
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

    int result = sem_wait(&self->native_handle.sem);
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

    int result = sem_trywait(&self->native_handle.sem);
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

    int result = sem_post(&self->native_handle.sem);
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

    int result = sem_getvalue(&self->native_handle.sem, value);
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

// 实现缺失的函数
static int64_t cond_exchange(InfraxSync* self, int64_t value) {
    return atomic_exchange(&self->value, value);
}

static bool cond_compare_exchange(InfraxSync* self, int64_t* expected, int64_t desired) {
    return atomic_compare_exchange_strong(&self->value, expected, desired);
}

static InfraxError cond_fetch_add(InfraxSync* self, int64_t value) {
    atomic_fetch_add(&self->value, value);
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError cond_fetch_sub(InfraxSync* self, int64_t value) {
    atomic_fetch_sub(&self->value, value);
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError cond_fetch_and(InfraxSync* self, int64_t value) {
    atomic_fetch_and(&self->value, value);
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError cond_fetch_or(InfraxSync* self, int64_t value) {
    atomic_fetch_or(&self->value, value);
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError cond_fetch_xor(InfraxSync* self, int64_t value) {
    atomic_fetch_xor(&self->value, value);
    return INFRAX_ERROR_OK_STRUCT;
}

// 实现初始化函数
static InfraxError mutex_init(InfraxSync* self) {
    if (pthread_mutex_init(&self->native_handle.mutex, NULL) != 0) {
        return make_error(INFRAX_ERROR_SYNC_INIT_FAILED, "Failed to initialize mutex");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError rwlock_init(InfraxSync* self) {
    if (pthread_rwlock_init(&self->native_handle.rwlock, NULL) != 0) {
        return make_error(INFRAX_ERROR_SYNC_INIT_FAILED, "Failed to initialize rwlock");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError spinlock_init(InfraxSync* self) {
    if (pthread_spin_init(&self->native_handle.spin, PTHREAD_PROCESS_PRIVATE) != 0) {
        return make_error(INFRAX_ERROR_SYNC_INIT_FAILED, "Failed to initialize spinlock");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError semaphore_init(InfraxSync* self) {
    if (sem_init(&self->native_handle.sem, 0, 0) != 0) {
        return make_error(INFRAX_ERROR_SYNC_INIT_FAILED, "Failed to initialize semaphore");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError condition_init(InfraxSync* self) {
    if (pthread_cond_init(&self->native_handle.cond, NULL) != 0) {
        return make_error(INFRAX_ERROR_SYNC_INIT_FAILED, "Failed to initialize condition");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError atomic_init_sync(InfraxSync* self) {
    atomic_init(&self->value, 0);
    return INFRAX_ERROR_OK_STRUCT;
}
