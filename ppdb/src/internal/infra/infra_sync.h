/*
 * infra_sync.h - Synchronization Primitives Interface
 */

#ifndef PPDB_INFRA_SYNC_H
#define PPDB_INFRA_SYNC_H

#include "cosmopolitan.h"
#include "internal/infra/infra.h"

//-----------------------------------------------------------------------------
// Thread Management
//-----------------------------------------------------------------------------

typedef void (*ppdb_thread_func_t)(void*);

typedef enum {
    PPDB_THREAD_INIT,
    PPDB_THREAD_RUNNING,
    PPDB_THREAD_STOPPING,
    PPDB_THREAD_STOPPED
} ppdb_thread_state_t;

typedef struct {
    pthread_t thread;
    ppdb_thread_state_t state;
    ppdb_thread_func_t func;
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
} ppdb_thread_t;

ppdb_error_t ppdb_thread_create(ppdb_thread_t** thread, 
                               ppdb_thread_func_t func, void* arg);
ppdb_error_t ppdb_thread_join(ppdb_thread_t* thread);
ppdb_error_t ppdb_thread_detach(ppdb_thread_t* thread);
ppdb_error_t ppdb_thread_destroy(ppdb_thread_t* thread);
ppdb_error_t ppdb_thread_set_name(ppdb_thread_t* thread, const char* name);
ppdb_error_t ppdb_thread_get_stats(ppdb_thread_t* thread);

//-----------------------------------------------------------------------------
// Mutex Operations
//-----------------------------------------------------------------------------

typedef struct {
    pthread_mutex_t mutex;
} ppdb_mutex_t;

ppdb_error_t ppdb_mutex_create(ppdb_mutex_t** mutex);
ppdb_error_t ppdb_mutex_destroy(ppdb_mutex_t* mutex);
ppdb_error_t ppdb_mutex_lock(ppdb_mutex_t* mutex);
ppdb_error_t ppdb_mutex_unlock(ppdb_mutex_t* mutex);
ppdb_error_t ppdb_mutex_trylock(ppdb_mutex_t* mutex);

//-----------------------------------------------------------------------------
// Condition Variables
//-----------------------------------------------------------------------------

typedef struct {
    pthread_cond_t cond;
} ppdb_cond_t;

ppdb_error_t ppdb_cond_create(ppdb_cond_t** cond);
ppdb_error_t ppdb_cond_destroy(ppdb_cond_t* cond);
ppdb_error_t ppdb_cond_wait(ppdb_cond_t* cond, ppdb_mutex_t* mutex);
ppdb_error_t ppdb_cond_signal(ppdb_cond_t* cond);
ppdb_error_t ppdb_cond_broadcast(ppdb_cond_t* cond);
ppdb_error_t ppdb_cond_timedwait(ppdb_cond_t* cond, ppdb_mutex_t* mutex, const struct timespec* abstime);

//-----------------------------------------------------------------------------
// Read-Write Locks
//-----------------------------------------------------------------------------

typedef struct {
    pthread_rwlock_t rwlock;
} ppdb_rwlock_t;

ppdb_error_t ppdb_rwlock_create(ppdb_rwlock_t** rwlock);
ppdb_error_t ppdb_rwlock_destroy(ppdb_rwlock_t* rwlock);
ppdb_error_t ppdb_rwlock_rdlock(ppdb_rwlock_t* rwlock);
ppdb_error_t ppdb_rwlock_wrlock(ppdb_rwlock_t* rwlock);
ppdb_error_t ppdb_rwlock_unlock(ppdb_rwlock_t* rwlock);

//-----------------------------------------------------------------------------
// Utility Functions
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_yield(void);
ppdb_error_t ppdb_sleep(uint32_t milliseconds);

#endif /* PPDB_INFRA_SYNC_H */ 