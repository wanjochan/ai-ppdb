#ifndef MOCK_PLATFORM_H
#define MOCK_PLATFORM_H

#include "cosmopolitan.h"
#include "internal/infra/infra_core.h"

// Time functions
infra_time_t mock_time_monotonic(void);

// Thread functions
int mock_thread_create(infra_thread_t* thread, infra_thread_attr_t* attr,
                      infra_thread_func_t func, void* arg);
int mock_thread_join(infra_thread_t thread, void** retval);
int mock_thread_detach(infra_thread_t thread);

// Mutex functions
int mock_mutex_init(infra_mutex_t* mutex, infra_mutex_attr_t* attr);
int mock_mutex_lock(infra_mutex_t* mutex);
int mock_mutex_unlock(infra_mutex_t* mutex);
int mock_mutex_destroy(infra_mutex_t* mutex);

// Condition variable functions
int mock_cond_init(infra_cond_t* cond, infra_cond_attr_t* attr);
int mock_cond_wait(infra_cond_t* cond, infra_mutex_t* mutex);
int mock_cond_signal(infra_cond_t* cond);
int mock_cond_broadcast(infra_cond_t* cond);
int mock_cond_destroy(infra_cond_t* cond);

#endif /* MOCK_PLATFORM_H */ 