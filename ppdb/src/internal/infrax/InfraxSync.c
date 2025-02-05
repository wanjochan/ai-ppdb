#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <time.h>
#include "InfraxSync.h"
#include "InfraxMemory.h"
#include "InfraxCore.h"

//-----------------------------------------------------------------------------
// Mutex Implementation
//-----------------------------------------------------------------------------

// Forward declarations of instance methods
static InfraxError mutex_lock(InfraxMutex* self);
static InfraxError mutex_try_lock(InfraxMutex* self);
static InfraxError mutex_unlock(InfraxMutex* self);

// Constructor implementation
static InfraxMutex* infrax_mutex_new(void) {
    // 获取全局内存管理器
    InfraxMemoryConfig mem_config = {
        .initial_size = 64 * 1024,
        .use_gc = true,
        .use_pool = true,
        .gc_threshold = 32 * 1024
    };
    InfraxMemory* memory = InfraxMemory_CLASS.new(&mem_config);
    if (!memory) {
        return NULL;
    }

    InfraxMutex* self = memory->alloc(memory, sizeof(InfraxMutex));
    if (!self) {
        InfraxMemory_CLASS.free(memory);
        return NULL;
    }

    // Initialize mutex
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    
    int result = pthread_mutex_init(&self->native_handle, &attr);
    pthread_mutexattr_destroy(&attr);
    
    if (result != 0) {
        memory->dealloc(memory, self);
        InfraxMemory_CLASS.free(memory);
        return NULL;
    }

    // Initialize instance
    self->klass = &InfraxMutex_CLASS;
    self->is_initialized = true;

    // Set instance methods
    self->lock = mutex_lock;
    self->try_lock = mutex_try_lock;
    self->unlock = mutex_unlock;

    return self;
}

// Destructor implementation
static void infrax_mutex_free(InfraxMutex* self) {
    if (!self) {
        return;
    }

    // 获取全局内存管理器
    InfraxMemoryConfig mem_config = {
        .initial_size = 64 * 1024,
        .use_gc = true,
        .use_pool = true,
        .gc_threshold = 32 * 1024
    };
    InfraxMemory* memory = InfraxMemory_CLASS.new(&mem_config);
    if (!memory) {
        return;
    }

    if (self->is_initialized) {
        pthread_mutex_destroy(&self->native_handle);
    }

    memory->dealloc(memory, self);
    InfraxMemory_CLASS.free(memory);
}

// Instance methods implementation
static InfraxError mutex_lock(InfraxMutex* self) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid mutex");
    }

    int result = pthread_mutex_lock(&self->native_handle);
    if (result != 0) {
        return core->new_error(INFRAX_ERROR_SYNC_LOCK_FAILED, "Failed to lock mutex");
    }

    return core->new_error(0, "");
}

static InfraxError mutex_try_lock(InfraxMutex* self) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid mutex");
    }

    int result = pthread_mutex_trylock(&self->native_handle);
    if (result == EBUSY) {
        return core->new_error(INFRAX_ERROR_SYNC_TIMEOUT, "Mutex is locked");
    } else if (result != 0) {
        return core->new_error(INFRAX_ERROR_SYNC_LOCK_FAILED, "Failed to try lock mutex");
    }

    return core->new_error(0, "");
}

static InfraxError mutex_unlock(InfraxMutex* self) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid mutex");
    }

    int result = pthread_mutex_unlock(&self->native_handle);
    if (result != 0) {
        return core->new_error(INFRAX_ERROR_SYNC_UNLOCK_FAILED, "Failed to unlock mutex");
    }

    return core->new_error(0, "");
}

// The class instance
const InfraxMutexClass InfraxMutex_CLASS = {
    .new = infrax_mutex_new,
    .free = infrax_mutex_free
};

//-----------------------------------------------------------------------------
// RWLock Implementation
//-----------------------------------------------------------------------------

// Forward declarations of instance methods
static InfraxError rwlock_read_lock(InfraxRWLock* self);
static InfraxError rwlock_try_read_lock(InfraxRWLock* self);
static InfraxError rwlock_read_unlock(InfraxRWLock* self);
static InfraxError rwlock_write_lock(InfraxRWLock* self);
static InfraxError rwlock_try_write_lock(InfraxRWLock* self);
static InfraxError rwlock_write_unlock(InfraxRWLock* self);

// Constructor implementation
static InfraxRWLock* infrax_rwlock_new(void) {
    // 获取全局内存管理器
    InfraxMemoryConfig mem_config = {
        .initial_size = 64 * 1024,
        .use_gc = true,
        .use_pool = true,
        .gc_threshold = 32 * 1024
    };
    InfraxMemory* memory = InfraxMemory_CLASS.new(&mem_config);
    if (!memory) {
        return NULL;
    }

    InfraxRWLock* self = memory->alloc(memory, sizeof(InfraxRWLock));
    if (!self) {
        InfraxMemory_CLASS.free(memory);
        return NULL;
    }

    // Initialize rwlock
    pthread_rwlockattr_t attr;
    pthread_rwlockattr_init(&attr);
    pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_PRIVATE);
    
    int result = pthread_rwlock_init(&self->native_handle, &attr);
    pthread_rwlockattr_destroy(&attr);
    
    if (result != 0) {
        memory->dealloc(memory, self);
        InfraxMemory_CLASS.free(memory);
        return NULL;
    }

    // Initialize instance
    self->klass = &InfraxRWLock_CLASS;
    self->is_initialized = true;

    // Set instance methods
    self->read_lock = rwlock_read_lock;
    self->try_read_lock = rwlock_try_read_lock;
    self->read_unlock = rwlock_read_unlock;
    self->write_lock = rwlock_write_lock;
    self->try_write_lock = rwlock_try_write_lock;
    self->write_unlock = rwlock_write_unlock;

    return self;
}

// Destructor implementation
static void infrax_rwlock_free(InfraxRWLock* self) {
    if (!self) {
        return;
    }

    // 获取全局内存管理器
    InfraxMemoryConfig mem_config = {
        .initial_size = 64 * 1024,
        .use_gc = true,
        .use_pool = true,
        .gc_threshold = 32 * 1024
    };
    InfraxMemory* memory = InfraxMemory_CLASS.new(&mem_config);
    if (!memory) {
        return;
    }

    if (self->is_initialized) {
        pthread_rwlock_destroy(&self->native_handle);
    }

    memory->dealloc(memory, self);
    InfraxMemory_CLASS.free(memory);
}

// Instance methods implementation
static InfraxError rwlock_read_lock(InfraxRWLock* self) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid rwlock");
    }

    int result = pthread_rwlock_rdlock(&self->native_handle);
    if (result != 0) {
        return core->new_error(INFRAX_ERROR_SYNC_LOCK_FAILED, "Failed to acquire read lock");
    }

    return core->new_error(0, "");
}

static InfraxError rwlock_try_read_lock(InfraxRWLock* self) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid rwlock");
    }

    int result = pthread_rwlock_tryrdlock(&self->native_handle);
    if (result == EBUSY) {
        return core->new_error(INFRAX_ERROR_SYNC_TIMEOUT, "RWLock is locked");
    } else if (result != 0) {
        return core->new_error(INFRAX_ERROR_SYNC_LOCK_FAILED, "Failed to try read lock");
    }

    return core->new_error(0, "");
}

static InfraxError rwlock_read_unlock(InfraxRWLock* self) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid rwlock");
    }

    int result = pthread_rwlock_unlock(&self->native_handle);
    if (result != 0) {
        return core->new_error(INFRAX_ERROR_SYNC_UNLOCK_FAILED, "Failed to release read lock");
    }

    return core->new_error(0, "");
}

static InfraxError rwlock_write_lock(InfraxRWLock* self) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid rwlock");
    }

    int result = pthread_rwlock_wrlock(&self->native_handle);
    if (result != 0) {
        return core->new_error(INFRAX_ERROR_SYNC_LOCK_FAILED, "Failed to acquire write lock");
    }

    return core->new_error(0, "");
}

static InfraxError rwlock_try_write_lock(InfraxRWLock* self) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid rwlock");
    }

    int result = pthread_rwlock_trywrlock(&self->native_handle);
    if (result == EBUSY) {
        return core->new_error(INFRAX_ERROR_SYNC_TIMEOUT, "RWLock is locked");
    } else if (result != 0) {
        return core->new_error(INFRAX_ERROR_SYNC_LOCK_FAILED, "Failed to try write lock");
    }

    return core->new_error(0, "");
}

static InfraxError rwlock_write_unlock(InfraxRWLock* self) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid rwlock");
    }

    int result = pthread_rwlock_unlock(&self->native_handle);
    if (result != 0) {
        return core->new_error(INFRAX_ERROR_SYNC_UNLOCK_FAILED, "Failed to release write lock");
    }

    return core->new_error(0, "");
}

// The class instance
const InfraxRWLockClass InfraxRWLock_CLASS = {
    .new = infrax_rwlock_new,
    .free = infrax_rwlock_free
};

//-----------------------------------------------------------------------------
// Spinlock Implementation
//-----------------------------------------------------------------------------

// Forward declarations of instance methods
static InfraxError spinlock_lock(InfraxSpinlock* self);
static InfraxError spinlock_try_lock(InfraxSpinlock* self);
static InfraxError spinlock_unlock(InfraxSpinlock* self);

// Constructor implementation
static InfraxSpinlock* infrax_spinlock_new(void) {
    // 获取全局内存管理器
    InfraxMemoryConfig mem_config = {
        .initial_size = 64 * 1024,
        .use_gc = true,
        .use_pool = true,
        .gc_threshold = 32 * 1024
    };
    InfraxMemory* memory = InfraxMemory_CLASS.new(&mem_config);
    if (!memory) {
        return NULL;
    }

    InfraxSpinlock* self = memory->alloc(memory, sizeof(InfraxSpinlock));
    if (!self) {
        InfraxMemory_CLASS.free(memory);
        return NULL;
    }

    // Initialize spinlock
    int result = pthread_spin_init(&self->native_handle, PTHREAD_PROCESS_PRIVATE);
    if (result != 0) {
        memory->dealloc(memory, self);
        InfraxMemory_CLASS.free(memory);
        return NULL;
    }

    // Initialize instance
    self->klass = &InfraxSpinlock_CLASS;
    self->is_initialized = true;

    // Set instance methods
    self->lock = spinlock_lock;
    self->try_lock = spinlock_try_lock;
    self->unlock = spinlock_unlock;

    return self;
}

// Destructor implementation
static void infrax_spinlock_free(InfraxSpinlock* self) {
    if (!self) {
        return;
    }

    // 获取全局内存管理器
    InfraxMemoryConfig mem_config = {
        .initial_size = 64 * 1024,
        .use_gc = true,
        .use_pool = true,
        .gc_threshold = 32 * 1024
    };
    InfraxMemory* memory = InfraxMemory_CLASS.new(&mem_config);
    if (!memory) {
        return;
    }

    if (self->is_initialized) {
        pthread_spin_destroy(&self->native_handle);
    }

    memory->dealloc(memory, self);
    InfraxMemory_CLASS.free(memory);
}

// Instance methods implementation
static InfraxError spinlock_lock(InfraxSpinlock* self) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid spinlock");
    }

    int result = pthread_spin_lock(&self->native_handle);
    if (result != 0) {
        return core->new_error(INFRAX_ERROR_SYNC_LOCK_FAILED, "Failed to acquire spinlock");
    }

    return core->new_error(0, "");
}

static InfraxError spinlock_try_lock(InfraxSpinlock* self) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid spinlock");
    }

    int result = pthread_spin_trylock(&self->native_handle);
    if (result == EBUSY) {
        return core->new_error(INFRAX_ERROR_SYNC_TIMEOUT, "Spinlock is locked");
    } else if (result != 0) {
        return core->new_error(INFRAX_ERROR_SYNC_LOCK_FAILED, "Failed to try spinlock");
    }

    return core->new_error(0, "");
}

static InfraxError spinlock_unlock(InfraxSpinlock* self) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid spinlock");
    }

    int result = pthread_spin_unlock(&self->native_handle);
    if (result != 0) {
        return core->new_error(INFRAX_ERROR_SYNC_UNLOCK_FAILED, "Failed to release spinlock");
    }

    return core->new_error(0, "");
}

// The class instance
const InfraxSpinlockClass InfraxSpinlock_CLASS = {
    .new = infrax_spinlock_new,
    .free = infrax_spinlock_free
};

//-----------------------------------------------------------------------------
// Semaphore Implementation
//-----------------------------------------------------------------------------

// Forward declarations of instance methods
static InfraxError semaphore_wait(InfraxSemaphore* self);
static InfraxError semaphore_try_wait(InfraxSemaphore* self);
static InfraxError semaphore_post(InfraxSemaphore* self);
static InfraxError semaphore_get_value(InfraxSemaphore* self, int* value);

// Constructor implementation
static InfraxSemaphore* infrax_semaphore_new(unsigned int initial_value) {
    // 获取全局内存管理器
    InfraxMemoryConfig mem_config = {
        .initial_size = 64 * 1024,
        .use_gc = true,
        .use_pool = true,
        .gc_threshold = 32 * 1024
    };
    InfraxMemory* memory = InfraxMemory_CLASS.new(&mem_config);
    if (!memory) {
        return NULL;
    }

    InfraxSemaphore* self = memory->alloc(memory, sizeof(InfraxSemaphore));
    if (!self) {
        InfraxMemory_CLASS.free(memory);
        return NULL;
    }

    // Initialize semaphore
    int result = sem_init(&self->native_handle, 0, initial_value);
    if (result != 0) {
        memory->dealloc(memory, self);
        InfraxMemory_CLASS.free(memory);
        return NULL;
    }

    // Initialize instance
    self->klass = &InfraxSemaphore_CLASS;
    self->is_initialized = true;

    // Set instance methods
    self->wait = semaphore_wait;
    self->try_wait = semaphore_try_wait;
    self->post = semaphore_post;
    self->get_value = semaphore_get_value;

    return self;
}

// Destructor implementation
static void infrax_semaphore_free(InfraxSemaphore* self) {
    if (!self) {
        return;
    }

    // 获取全局内存管理器
    InfraxMemoryConfig mem_config = {
        .initial_size = 64 * 1024,
        .use_gc = true,
        .use_pool = true,
        .gc_threshold = 32 * 1024
    };
    InfraxMemory* memory = InfraxMemory_CLASS.new(&mem_config);
    if (!memory) {
        return;
    }

    if (self->is_initialized) {
        sem_destroy(&self->native_handle);
    }

    memory->dealloc(memory, self);
    InfraxMemory_CLASS.free(memory);
}

// Instance methods implementation
static InfraxError semaphore_wait(InfraxSemaphore* self) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid semaphore");
    }

    int result = sem_wait(&self->native_handle);
    if (result != 0) {
        return core->new_error(INFRAX_ERROR_SYNC_WAIT_FAILED, "Failed to wait on semaphore");
    }

    return core->new_error(0, "");
}

static InfraxError semaphore_try_wait(InfraxSemaphore* self) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid semaphore");
    }

    int result = sem_trywait(&self->native_handle);
    if (result == -1) {
        if (errno == EAGAIN) {
            return core->new_error(INFRAX_ERROR_SYNC_TIMEOUT, "Semaphore is not available");
        } else {
            return core->new_error(INFRAX_ERROR_SYNC_WAIT_FAILED, "Failed to try wait on semaphore");
        }
    }

    return core->new_error(0, "");
}

static InfraxError semaphore_post(InfraxSemaphore* self) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid semaphore");
    }

    int result = sem_post(&self->native_handle);
    if (result != 0) {
        return core->new_error(INFRAX_ERROR_SYNC_SIGNAL_FAILED, "Failed to post semaphore");
    }

    return core->new_error(0, "");
}

static InfraxError semaphore_get_value(InfraxSemaphore* self, int* value) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized || !value) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid semaphore or value pointer");
    }

    int result = sem_getvalue(&self->native_handle, value);
    if (result != 0) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Failed to get semaphore value");
    }

    return core->new_error(0, "");
}

// The class instance
const InfraxSemaphoreClass InfraxSemaphore_CLASS = {
    .new = infrax_semaphore_new,
    .free = infrax_semaphore_free
};

//-----------------------------------------------------------------------------
// Condition Variable Implementation
//-----------------------------------------------------------------------------

// Forward declarations of instance methods
static InfraxError cond_wait(InfraxCond* self, InfraxMutex* mutex);
static InfraxError cond_timedwait(InfraxCond* self, InfraxMutex* mutex, InfraxTime timeout_ms);
static InfraxError cond_signal(InfraxCond* self);
static InfraxError cond_broadcast(InfraxCond* self);

// Constructor implementation
static InfraxCond* infrax_cond_new(void) {
    // 获取全局内存管理器
    InfraxMemoryConfig mem_config = {
        .initial_size = 64 * 1024,
        .use_gc = true,
        .use_pool = true,
        .gc_threshold = 32 * 1024
    };
    InfraxMemory* memory = InfraxMemory_CLASS.new(&mem_config);
    if (!memory) {
        return NULL;
    }

    InfraxCond* self = memory->alloc(memory, sizeof(InfraxCond));
    if (!self) {
        InfraxMemory_CLASS.free(memory);
        return NULL;
    }

    // Initialize condition variable
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setpshared(&attr, PTHREAD_PROCESS_PRIVATE);
    
    int result = pthread_cond_init(&self->native_handle, &attr);
    pthread_condattr_destroy(&attr);
    
    if (result != 0) {
        memory->dealloc(memory, self);
        InfraxMemory_CLASS.free(memory);
        return NULL;
    }

    // Initialize instance
    self->klass = &InfraxCond_CLASS;
    self->is_initialized = true;

    // Set instance methods
    self->wait = cond_wait;
    self->timedwait = cond_timedwait;
    self->signal = cond_signal;
    self->broadcast = cond_broadcast;

    return self;
}

// Destructor implementation
static void infrax_cond_free(InfraxCond* self) {
    if (!self) {
        return;
    }

    // 获取全局内存管理器
    InfraxMemoryConfig mem_config = {
        .initial_size = 64 * 1024,
        .use_gc = true,
        .use_pool = true,
        .gc_threshold = 32 * 1024
    };
    InfraxMemory* memory = InfraxMemory_CLASS.new(&mem_config);
    if (!memory) {
        return;
    }

    if (self->is_initialized) {
        pthread_cond_destroy(&self->native_handle);
    }

    memory->dealloc(memory, self);
    InfraxMemory_CLASS.free(memory);
}

// Instance methods implementation
static InfraxError cond_wait(InfraxCond* self, InfraxMutex* mutex) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized || !mutex || !mutex->is_initialized) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid condition variable or mutex");
    }

    int result = pthread_cond_wait(&self->native_handle, &mutex->native_handle);
    if (result != 0) {
        return core->new_error(INFRAX_ERROR_SYNC_WAIT_FAILED, "Failed to wait on condition variable");
    }

    return core->new_error(0, "");
}

static InfraxError cond_timedwait(InfraxCond* self, InfraxMutex* mutex, InfraxTime timeout_ms) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized || !mutex || !mutex->is_initialized) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid condition variable or mutex");
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000;
    }

    int result = pthread_cond_timedwait(&self->native_handle, &mutex->native_handle, &ts);
    if (result == ETIMEDOUT) {
        return core->new_error(INFRAX_ERROR_SYNC_TIMEOUT, "Condition variable wait timed out");
    } else if (result != 0) {
        return core->new_error(INFRAX_ERROR_SYNC_WAIT_FAILED, "Failed to wait on condition variable");
    }

    return core->new_error(0, "");
}

static InfraxError cond_signal(InfraxCond* self) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid condition variable");
    }

    int result = pthread_cond_signal(&self->native_handle);
    if (result != 0) {
        return core->new_error(INFRAX_ERROR_SYNC_SIGNAL_FAILED, "Failed to signal condition variable");
    }

    return core->new_error(0, "");
}

static InfraxError cond_broadcast(InfraxCond* self) {
    InfraxCore* core = get_global_infrax_core();
    
    if (!self || !self->is_initialized) {
        return core->new_error(INFRAX_ERROR_SYNC_INVALID_ARGUMENT, "Invalid condition variable");
    }

    int result = pthread_cond_broadcast(&self->native_handle);
    if (result != 0) {
        return core->new_error(INFRAX_ERROR_SYNC_SIGNAL_FAILED, "Failed to broadcast condition variable");
    }

    return core->new_error(0, "");
}

// The class instance
const InfraxCondClass InfraxCond_CLASS = {
    .new = infrax_cond_new,
    .free = infrax_cond_free
}; 