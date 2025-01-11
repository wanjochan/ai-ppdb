#ifndef PPDB_TEST_INFRA_MOCK_PLATFORM_H
#define PPDB_TEST_INFRA_MOCK_PLATFORM_H

#include "framework/mock_framework/mock_framework.h"
#include "internal/infra/infra.h"
#include "internal/infra/infra_platform.h"

// Thread mocks
MOCK_FUNC(infra_error_t, infra_thread_create, infra_thread_t* thread, infra_thread_func_t func, void* arg);
MOCK_FUNC(infra_error_t, infra_thread_join, infra_thread_t thread);
MOCK_FUNC(infra_error_t, infra_thread_detach, infra_thread_t thread);

// Mutex mocks
MOCK_FUNC(infra_error_t, infra_mutex_create, infra_mutex_t* mutex);
MOCK_FUNC(infra_error_t, infra_mutex_destroy, infra_mutex_t mutex);
MOCK_FUNC(infra_error_t, infra_mutex_lock, infra_mutex_t mutex);
MOCK_FUNC(infra_error_t, infra_mutex_unlock, infra_mutex_t mutex);

// Condition variable mocks
MOCK_FUNC(infra_error_t, infra_cond_init, infra_cond_t* cond);
MOCK_FUNC(infra_error_t, infra_cond_destroy, infra_cond_t cond);
MOCK_FUNC(infra_error_t, infra_cond_wait, infra_cond_t cond, infra_mutex_t mutex);
MOCK_FUNC(infra_error_t, infra_cond_signal, infra_cond_t cond);

// Time mocks
MOCK_FUNC(infra_time_t, infra_time_now, void);
MOCK_FUNC(infra_time_t, infra_time_monotonic, void);
MOCK_FUNC(void, infra_time_sleep, uint32_t ms);

// Initialize platform mocks
mock_error_t mock_platform_init(void);

// Cleanup platform mocks
void mock_platform_cleanup(void);

#endif // PPDB_TEST_INFRA_MOCK_PLATFORM_H