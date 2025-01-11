#include "test/white/framework/test_framework.h"
#include "test/white/framework/mock_framework.h"
#include "test/white/infra/mock_platform.h"

// Time functions
infra_time_t mock_time_monotonic(void) {
    mock_function_call("mock_time_monotonic");
    return mock_return_value("mock_time_monotonic");
}

// Thread functions
int mock_thread_create(infra_thread_t* thread, infra_thread_attr_t* attr,
                      infra_thread_func_t func, void* arg) {
    mock_function_call("mock_thread_create");
    mock_param_ptr("thread", thread);
    mock_param_ptr("attr", attr);
    mock_param_ptr("func", (void*)func);
    mock_param_ptr("arg", arg);
    return mock_return_value("mock_thread_create");
}

int mock_thread_join(infra_thread_t thread, void** retval) {
    mock_function_call("mock_thread_join");
    mock_param_ptr("thread", thread);
    mock_param_ptr("retval", retval);
    return mock_return_value("mock_thread_join");
}

int mock_thread_detach(infra_thread_t thread) {
    mock_function_call("mock_thread_detach");
    mock_param_ptr("thread", thread);
    return mock_return_value("mock_thread_detach");
}

// Mutex functions
int mock_mutex_init(infra_mutex_t* mutex, infra_mutex_attr_t* attr) {
    mock_function_call("mock_mutex_init");
    mock_param_ptr("mutex", mutex);
    mock_param_ptr("attr", attr);
    return mock_return_value("mock_mutex_init");
}

int mock_mutex_lock(infra_mutex_t* mutex) {
    mock_function_call("mock_mutex_lock");
    mock_param_ptr("mutex", mutex);
    return mock_return_value("mock_mutex_lock");
}

int mock_mutex_unlock(infra_mutex_t* mutex) {
    mock_function_call("mock_mutex_unlock");
    mock_param_ptr("mutex", mutex);
    return mock_return_value("mock_mutex_unlock");
}

int mock_mutex_destroy(infra_mutex_t* mutex) {
    mock_function_call("mock_mutex_destroy");
    mock_param_ptr("mutex", mutex);
    return mock_return_value("mock_mutex_destroy");
}

// Condition variable functions
int mock_cond_init(infra_cond_t* cond, infra_cond_attr_t* attr) {
    mock_function_call("mock_cond_init");
    mock_param_ptr("cond", cond);
    mock_param_ptr("attr", attr);
    return mock_return_value("mock_cond_init");
}

int mock_cond_wait(infra_cond_t* cond, infra_mutex_t* mutex) {
    mock_function_call("mock_cond_wait");
    mock_param_ptr("cond", cond);
    mock_param_ptr("mutex", mutex);
    return mock_return_value("mock_cond_wait");
}

int mock_cond_signal(infra_cond_t* cond) {
    mock_function_call("mock_cond_signal");
    mock_param_ptr("cond", cond);
    return mock_return_value("mock_cond_signal");
}

int mock_cond_broadcast(infra_cond_t* cond) {
    mock_function_call("mock_cond_broadcast");
    mock_param_ptr("cond", cond);
    return mock_return_value("mock_cond_broadcast");
}

int mock_cond_destroy(infra_cond_t* cond) {
    mock_function_call("mock_cond_destroy");
    mock_param_ptr("cond", cond);
    return mock_return_value("mock_cond_destroy");
} 