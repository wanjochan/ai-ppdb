#include "InfraxCore.h"
#include "InfraxMemory.h"
#include "InfraxSync.h"

// Include system headers only in the implementation file
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include <stdatomic.h>
#include <stdbool.h>

// Helper macros to access native handles
#define GET_MUTEX(sync) ((pthread_mutex_t*)&((sync)->handle.data[0]))
#define GET_RWLOCK(sync) ((pthread_rwlock_t*)&((sync)->handle.data[0]))
#define GET_SPINLOCK(sync) ((pthread_spinlock_t*)&((sync)->handle.data[0]))
#define GET_SEMAPHORE(sync) ((sem_t*)&((sync)->handle.data[0]))
#define GET_CONDITION(sync) ((pthread_cond_t*)&((sync)->handle.data[0]))
#define GET_ATOMIC(sync) ((atomic_int_least64_t*)&((sync)->value))

// Forward declaration of static variables
static InfraxBool is_initialized = INFRAX_FALSE;
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
static InfraxI64 infrax_atomic_load(InfraxSync* self);
static void infrax_atomic_store(InfraxSync* self, InfraxI64 value);
static InfraxI64 infrax_atomic_exchange(InfraxSync* self, InfraxI64 value);
static InfraxBool infrax_atomic_compare_exchange(InfraxSync* self, InfraxI64* expected, InfraxI64 desired);
static InfraxI64 infrax_atomic_fetch_add(InfraxSync* self, InfraxI64 value);
static InfraxI64 infrax_atomic_fetch_sub(InfraxSync* self, InfraxI64 value);
static InfraxI64 infrax_atomic_fetch_and(InfraxSync* self, InfraxI64 value);
static InfraxI64 infrax_atomic_fetch_or(InfraxSync* self, InfraxI64 value);
static InfraxI64 infrax_atomic_fetch_xor(InfraxSync* self, InfraxI64 value);

// 添加缺失的函数声明
static InfraxI64 cond_exchange(InfraxSync* self, InfraxI64 value);
static InfraxBool cond_compare_exchange(InfraxSync* self, InfraxI64* expected, InfraxI64 desired);
static InfraxError cond_fetch_add(InfraxSync* self, InfraxI64 value);
static InfraxError cond_fetch_sub(InfraxSync* self, InfraxI64 value);
static InfraxError cond_fetch_and(InfraxSync* self, InfraxI64 value);
static InfraxError cond_fetch_or(InfraxSync* self, InfraxI64 value);
static InfraxError cond_fetch_xor(InfraxSync* self, InfraxI64 value);

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
            pthread_mutex_init(GET_MUTEX(sync), NULL);
            break;
        case INFRAX_SYNC_TYPE_CONDITION:
            pthread_cond_init(GET_CONDITION(sync), NULL);
            break;
        case INFRAX_SYNC_TYPE_RWLOCK:
            pthread_rwlock_init(GET_RWLOCK(sync), NULL);
            break;
        case INFRAX_SYNC_TYPE_SPINLOCK:
            pthread_spin_init(GET_SPINLOCK(sync), PTHREAD_PROCESS_PRIVATE);
            break;
        case INFRAX_SYNC_TYPE_SEMAPHORE:
            sem_init(GET_SEMAPHORE(sync), 0, 0);
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
            pthread_mutex_destroy(GET_MUTEX(sync));
            break;
        case INFRAX_SYNC_TYPE_CONDITION:
            pthread_cond_destroy(GET_CONDITION(sync));
            break;
        case INFRAX_SYNC_TYPE_RWLOCK:
            pthread_rwlock_destroy(GET_RWLOCK(sync));
            break;
        case INFRAX_SYNC_TYPE_SPINLOCK:
            pthread_spin_destroy(GET_SPINLOCK(sync));
            break;
        case INFRAX_SYNC_TYPE_SEMAPHORE:
            sem_destroy(GET_SEMAPHORE(sync));
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

    int result = pthread_mutex_lock(GET_MUTEX(self));
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

    int result = pthread_mutex_trylock(GET_MUTEX(self));
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

    int result = pthread_mutex_unlock(GET_MUTEX(self));
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

    int result = pthread_rwlock_rdlock(GET_RWLOCK(self));
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

    int result = pthread_rwlock_tryrdlock(GET_RWLOCK(self));
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

    int result = pthread_rwlock_unlock(GET_RWLOCK(self));
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

    int result = pthread_rwlock_wrlock(GET_RWLOCK(self));
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

    int result = pthread_rwlock_trywrlock(GET_RWLOCK(self));
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

    int result = pthread_rwlock_unlock(GET_RWLOCK(self));
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

    int result = pthread_spin_lock(GET_SPINLOCK(self));
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

    int result = pthread_spin_trylock(GET_SPINLOCK(self));
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

    int result = pthread_spin_unlock(GET_SPINLOCK(self));
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

    int result = sem_wait(GET_SEMAPHORE(self));
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

    int result = sem_trywait(GET_SEMAPHORE(self));
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
    int result = sem_post(GET_SEMAPHORE(self));
    if (result != 0) {
        return make_error(INFRAX_ERROR_SYNC_SIGNAL_FAILED, "Failed to post semaphore");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError semaphore_get_value(InfraxSync* self, int* value) {
    int result = sem_getvalue(GET_SEMAPHORE(self), value);
    if (result != 0) {
        return make_error(INFRAX_ERROR_SYNC_SIGNAL_FAILED, "Failed to get semaphore value");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

//-----------------------------------------------------------------------------
// Condition Variable Implementation
//-----------------------------------------------------------------------------

static InfraxError cond_wait(InfraxSync* self, InfraxSync* mutex) {
    int result = pthread_cond_wait(GET_CONDITION(self), GET_MUTEX(mutex));
    if (result != 0) {
        return make_error(INFRAX_ERROR_SYNC_WAIT_FAILED, "Failed to wait on condition");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError cond_timedwait(InfraxSync* self, InfraxSync* mutex, InfraxTime timeout_ms) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000;
    }
    
    int result = pthread_cond_timedwait(GET_CONDITION(self), GET_MUTEX(mutex), &ts);
    if (result == ETIMEDOUT) {
        return make_error(INFRAX_ERROR_SYNC_TIMEOUT, "Condition wait timed out");
    } else if (result != 0) {
        return make_error(INFRAX_ERROR_SYNC_WAIT_FAILED, "Failed to wait on condition");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError cond_signal(InfraxSync* self) {
    int result = pthread_cond_signal(GET_CONDITION(self));
    if (result != 0) {
        return make_error(INFRAX_ERROR_SYNC_SIGNAL_FAILED, "Failed to signal condition");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError cond_broadcast(InfraxSync* self) {
    int result = pthread_cond_broadcast(GET_CONDITION(self));
    if (result != 0) {
        return make_error(INFRAX_ERROR_SYNC_SIGNAL_FAILED, "Failed to broadcast condition");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

//-----------------------------------------------------------------------------
// Atomic Operations Implementation
//-----------------------------------------------------------------------------

static InfraxI64 infrax_atomic_load(InfraxSync* self) {
    return atomic_load(GET_ATOMIC(self));
}

static void infrax_atomic_store(InfraxSync* self, InfraxI64 value) {
    atomic_store(GET_ATOMIC(self), value);
}

static InfraxI64 infrax_atomic_exchange(InfraxSync* self, InfraxI64 value) {
    return atomic_exchange(GET_ATOMIC(self), value);
}

static InfraxBool infrax_atomic_compare_exchange(InfraxSync* self, InfraxI64* expected, InfraxI64 desired) {
    return atomic_compare_exchange_strong(GET_ATOMIC(self), expected, desired);
}

static InfraxI64 infrax_atomic_fetch_add(InfraxSync* self, InfraxI64 value) {
    return atomic_fetch_add(GET_ATOMIC(self), value);
}

static InfraxI64 infrax_atomic_fetch_sub(InfraxSync* self, InfraxI64 value) {
    return atomic_fetch_sub(GET_ATOMIC(self), value);
}

static InfraxI64 infrax_atomic_fetch_and(InfraxSync* self, InfraxI64 value) {
    return atomic_fetch_and(GET_ATOMIC(self), value);
}

static InfraxI64 infrax_atomic_fetch_or(InfraxSync* self, InfraxI64 value) {
    return atomic_fetch_or(GET_ATOMIC(self), value);
}

static InfraxI64 infrax_atomic_fetch_xor(InfraxSync* self, InfraxI64 value) {
    return atomic_fetch_xor(GET_ATOMIC(self), value);
}

// 实现缺失的函数
static InfraxI64 cond_exchange(InfraxSync* self, InfraxI64 value) {
    return atomic_exchange(GET_ATOMIC(self), value);
}

static InfraxBool cond_compare_exchange(InfraxSync* self, InfraxI64* expected, InfraxI64 desired) {
    return atomic_compare_exchange_strong(GET_ATOMIC(self), expected, desired);
}

static InfraxError cond_fetch_add(InfraxSync* self, InfraxI64 value) {
    atomic_fetch_add(GET_ATOMIC(self), value);
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError cond_fetch_sub(InfraxSync* self, InfraxI64 value) {
    atomic_fetch_sub(GET_ATOMIC(self), value);
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError cond_fetch_and(InfraxSync* self, InfraxI64 value) {
    atomic_fetch_and(GET_ATOMIC(self), value);
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError cond_fetch_or(InfraxSync* self, InfraxI64 value) {
    atomic_fetch_or(GET_ATOMIC(self), value);
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError cond_fetch_xor(InfraxSync* self, InfraxI64 value) {
    atomic_fetch_xor(GET_ATOMIC(self), value);
    return INFRAX_ERROR_OK_STRUCT;
}

// 实现初始化函数
static InfraxError mutex_init(InfraxSync* self) {
    if (pthread_mutex_init(GET_MUTEX(self), NULL) != 0) {
        return make_error(INFRAX_ERROR_SYNC_INIT_FAILED, "Failed to initialize mutex");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError rwlock_init(InfraxSync* self) {
    if (pthread_rwlock_init(GET_RWLOCK(self), NULL) != 0) {
        return make_error(INFRAX_ERROR_SYNC_INIT_FAILED, "Failed to initialize rwlock");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError spinlock_init(InfraxSync* self) {
    if (pthread_spin_init(GET_SPINLOCK(self), PTHREAD_PROCESS_PRIVATE) != 0) {
        return make_error(INFRAX_ERROR_SYNC_INIT_FAILED, "Failed to initialize spinlock");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError semaphore_init(InfraxSync* self) {
    if (sem_init(GET_SEMAPHORE(self), 0, 0) != 0) {
        return make_error(INFRAX_ERROR_SYNC_INIT_FAILED, "Failed to initialize semaphore");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError condition_init(InfraxSync* self) {
    if (pthread_cond_init(GET_CONDITION(self), NULL) != 0) {
        return make_error(INFRAX_ERROR_SYNC_INIT_FAILED, "Failed to initialize condition");
    }
    return INFRAX_ERROR_OK_STRUCT;
}

static InfraxError atomic_init_sync(InfraxSync* self) {
    if (!self) {
        return make_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid sync object");
    }
    
    // Initialize atomic value to 0
    atomic_init(GET_ATOMIC(self), 0);
    
    return INFRAX_ERROR_OK_STRUCT;
}
