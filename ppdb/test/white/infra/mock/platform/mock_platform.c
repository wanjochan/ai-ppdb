#include "mock_platform.h"
#include "internal/infra/infra_platform.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_error.h"

// Original function pointers
infra_error_t (*real_infra_thread_create)(infra_thread_t* thread, infra_thread_func_t func, void* arg);
infra_error_t (*real_infra_thread_join)(infra_thread_t thread);
infra_error_t (*real_infra_mutex_create)(infra_mutex_t* mutex);
infra_error_t (*real_infra_mutex_lock)(infra_mutex_t mutex);
infra_error_t (*real_infra_mutex_unlock)(infra_mutex_t mutex);
infra_error_t (*real_infra_cond_init)(infra_cond_t* cond);
infra_error_t (*real_infra_cond_wait)(infra_cond_t cond, infra_mutex_t mutex);
infra_error_t (*real_infra_cond_signal)(infra_cond_t cond);
infra_time_t (*real_infra_time_now)(void);
infra_time_t (*real_infra_time_monotonic)(void);

// Mock implementations
infra_error_t mock_infra_thread_create(infra_thread_t* thread, infra_thread_func_t func, void* arg) {
    mock_expectation_t* exp = mock_register_expectation();
    if (exp) {
        exp->actual_calls++;
        return (infra_error_t)(intptr_t)exp->return_value;
    }
    return real_infra_thread_create(thread, func, arg);
}

infra_error_t mock_infra_thread_join(infra_thread_t thread) {
    mock_expectation_t* exp = mock_register_expectation();
    if (exp) {
        exp->actual_calls++;
        return (infra_error_t)(intptr_t)exp->return_value;
    }
    return real_infra_thread_join(thread);
}

infra_error_t mock_infra_mutex_create(infra_mutex_t* mutex) {
    mock_expectation_t* exp = mock_register_expectation();
    if (exp) {
        exp->actual_calls++;
        return (infra_error_t)(intptr_t)exp->return_value;
    }
    return real_infra_mutex_create(mutex);
}

infra_error_t mock_infra_mutex_lock(infra_mutex_t mutex) {
    mock_expectation_t* exp = mock_register_expectation();
    if (exp) {
        exp->actual_calls++;
        return (infra_error_t)(intptr_t)exp->return_value;
    }
    return real_infra_mutex_lock(mutex);
}

infra_error_t mock_infra_mutex_unlock(infra_mutex_t mutex) {
    mock_expectation_t* exp = mock_register_expectation();
    if (exp) {
        exp->actual_calls++;
        return (infra_error_t)(intptr_t)exp->return_value;
    }
    return real_infra_mutex_unlock(mutex);
}

infra_error_t mock_infra_cond_init(infra_cond_t* cond) {
    mock_expectation_t* exp = mock_register_expectation();
    if (exp) {
        exp->actual_calls++;
        return (infra_error_t)(intptr_t)exp->return_value;
    }
    return real_infra_cond_init(cond);
}

infra_error_t mock_infra_cond_wait(infra_cond_t cond, infra_mutex_t mutex) {
    mock_expectation_t* exp = mock_register_expectation();
    if (exp) {
        exp->actual_calls++;
        return (infra_error_t)(intptr_t)exp->return_value;
    }
    return real_infra_cond_wait(cond, mutex);
}

infra_error_t mock_infra_cond_signal(infra_cond_t cond) {
    mock_expectation_t* exp = mock_register_expectation();
    if (exp) {
        exp->actual_calls++;
        return (infra_error_t)(intptr_t)exp->return_value;
    }
    return real_infra_cond_signal(cond);
}

infra_time_t mock_infra_time_now(void) {
    mock_expectation_t* exp = mock_register_expectation();
    if (exp) {
        exp->actual_calls++;
        return (infra_time_t)(intptr_t)exp->return_value;
    }
    return real_infra_time_now();
}

infra_time_t mock_infra_time_monotonic(void) {
    mock_expectation_t* exp = mock_register_expectation();
    if (exp) {
        exp->actual_calls++;
        return (infra_time_t)(intptr_t)exp->return_value;
    }
    return real_infra_time_monotonic();
}

void mock_platform_init(void) {
    // Store original function pointers
    real_infra_thread_create = infra_thread_create;
    real_infra_thread_join = infra_thread_join;
    real_infra_mutex_create = infra_mutex_create;
    real_infra_mutex_lock = infra_mutex_lock;
    real_infra_mutex_unlock = infra_mutex_unlock;
    real_infra_cond_init = infra_cond_init;
    real_infra_cond_wait = infra_cond_wait;
    real_infra_cond_signal = infra_cond_signal;
    real_infra_time_now = infra_time_now;
    real_infra_time_monotonic = infra_time_monotonic;
    
    // Initialize mock framework
    mock_framework_init();
}

void mock_platform_cleanup(void) {
    // Restore original function pointers
    infra_thread_create = real_infra_thread_create;
    infra_thread_join = real_infra_thread_join;
    infra_mutex_create = real_infra_mutex_create;
    infra_mutex_lock = real_infra_mutex_lock;
    infra_mutex_unlock = real_infra_mutex_unlock;
    infra_cond_init = real_infra_cond_init;
    infra_cond_wait = real_infra_cond_wait;
    infra_cond_signal = real_infra_cond_signal;
    infra_time_now = real_infra_time_now;
    infra_time_monotonic = real_infra_time_monotonic;
    
    // Cleanup mock framework
    mock_framework_cleanup();
}