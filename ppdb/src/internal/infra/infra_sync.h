/*
 * infra_sync.h - Synchronization Primitives Interface
 */

#ifndef INFRA_SYNC_H
#define INFRA_SYNC_H

#include "internal/infra/infra.h"

//-----------------------------------------------------------------------------
// Thread Management
//-----------------------------------------------------------------------------

typedef void (*infra_thread_func_t)(void*);

typedef enum {
    INFRA_THREAD_INIT,
    INFRA_THREAD_RUNNING,
    INFRA_THREAD_STOPPING,
    INFRA_THREAD_STOPPED
} infra_thread_state_t;

typedef struct {
    pthread_t thread;
    infra_thread_state_t state;
    infra_thread_func_t func;
    void* arg;
    char name[32];
    uint32_t flags;
    uint64_t start_time;
    uint64_t stop_time;
    uint64_t cpu_time;
    struct {
        uint64_t user_time;
        uint64_t system_time;
        uint64_t voluntary_switches;
        uint64_t involuntary_switches;
    } stats;
} infra_thread_t;

infra_error_t infra_thread_create(infra_thread_t** thread, 
                                 infra_thread_func_t func, void* arg);
infra_error_t infra_thread_join(infra_thread_t* thread);
infra_error_t infra_thread_detach(infra_thread_t* thread);
infra_error_t infra_thread_destroy(infra_thread_t* thread);
infra_error_t infra_thread_set_name(infra_thread_t* thread, const char* name);
infra_error_t infra_thread_get_stats(infra_thread_t* thread);

//-----------------------------------------------------------------------------
// Mutex Operations
//-----------------------------------------------------------------------------

typedef struct {
    pthread_mutex_t mutex;
} infra_mutex_t;

infra_error_t infra_mutex_create(infra_mutex_t** mutex);
infra_error_t infra_mutex_destroy(infra_mutex_t* mutex);
infra_error_t infra_mutex_lock(infra_mutex_t* mutex);
infra_error_t infra_mutex_unlock(infra_mutex_t* mutex);
infra_error_t infra_mutex_trylock(infra_mutex_t* mutex);

//-----------------------------------------------------------------------------
// Condition Variables
//-----------------------------------------------------------------------------

typedef struct {
    pthread_cond_t cond;
} infra_cond_t;

infra_error_t infra_cond_create(infra_cond_t** cond);
infra_error_t infra_cond_destroy(infra_cond_t* cond);
infra_error_t infra_cond_wait(infra_cond_t* cond, infra_mutex_t* mutex);
infra_error_t infra_cond_signal(infra_cond_t* cond);
infra_error_t infra_cond_broadcast(infra_cond_t* cond);
infra_error_t infra_cond_timedwait(infra_cond_t* cond, infra_mutex_t* mutex, const struct timespec* abstime);

//-----------------------------------------------------------------------------
// Read-Write Locks
//-----------------------------------------------------------------------------

typedef struct {
    pthread_rwlock_t rwlock;
} infra_rwlock_t;

infra_error_t infra_rwlock_create(infra_rwlock_t** rwlock);
infra_error_t infra_rwlock_destroy(infra_rwlock_t* rwlock);
infra_error_t infra_rwlock_rdlock(infra_rwlock_t* rwlock);
infra_error_t infra_rwlock_wrlock(infra_rwlock_t* rwlock);
infra_error_t infra_rwlock_unlock(infra_rwlock_t* rwlock);

//-----------------------------------------------------------------------------
// Utility Functions
//-----------------------------------------------------------------------------

infra_error_t infra_yield(void);
infra_error_t infra_sleep(uint32_t milliseconds);

//-----------------------------------------------------------------------------
// Synchronization Context
//-----------------------------------------------------------------------------

typedef struct infra_sync_context infra_sync_context_t;
typedef struct infra_sync_task infra_sync_task_t;
typedef void (*infra_sync_callback_t)(infra_sync_task_t* task, infra_error_t error);

struct infra_sync_task {
    infra_sync_callback_t callback;
    void* user_data;
    infra_stats_t* stats;
};

infra_error_t infra_sync_init(infra_sync_context_t** ctx);
void infra_sync_destroy(infra_sync_context_t* ctx);
infra_error_t infra_sync_submit(infra_sync_context_t* ctx,
                               infra_sync_task_t* task);
infra_error_t infra_sync_cancel(infra_sync_context_t* ctx,
                               infra_sync_task_t* task);
infra_error_t infra_sync_run(infra_sync_context_t* ctx);
infra_error_t infra_sync_stop(infra_sync_context_t* ctx);

#endif /* INFRA_SYNC_H */ 