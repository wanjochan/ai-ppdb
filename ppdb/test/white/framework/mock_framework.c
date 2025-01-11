#include "test/white/framework/mock_framework.h"
#include "test/white/framework/test_framework.h"
#include "internal/infra/infra_core.h"

#define MAX_MOCK_CALLS 1024
#define MAX_PARAMS_PER_CALL 16

typedef struct {
    const char* name;
    union {
        uint64_t value;
        void* ptr;
        const char* str;
    } value;
    int verified;
} mock_param_t;

typedef struct {
    const char* function_name;
    mock_param_t params[MAX_PARAMS_PER_CALL];
    size_t param_count;
    union {
        uint64_t value;
        void* ptr;
    } return_val;
    int called;
    int returned;
} mock_call_t;

static mock_call_t g_mock_calls[MAX_MOCK_CALLS];
static size_t g_mock_call_count = 0;
static size_t g_mock_call_index = 0;
static size_t g_current_call = 0;

void mock_init(void) {
    g_mock_call_count = 0;
    g_mock_call_index = 0;
    g_current_call = 0;
}

void mock_cleanup(void) {
    g_mock_call_count = 0;
    g_mock_call_index = 0;
    g_current_call = 0;
}

void mock_verify(void) {
    for (size_t i = 0; i < g_mock_call_count; i++) {
        if (!g_mock_calls[i].called) {
            TEST_ASSERT_MSG_VOID(false, "Expected function %s was not called",
                g_mock_calls[i].function_name);
        }
        if (!g_mock_calls[i].returned) {
            TEST_ASSERT_MSG_VOID(false, "Function %s was called but return value not used",
                g_mock_calls[i].function_name);
        }
        for (size_t j = 0; j < g_mock_calls[i].param_count; j++) {
            if (!g_mock_calls[i].params[j].verified) {
                TEST_ASSERT_MSG_VOID(false, "Parameter %s of function %s was not verified",
                    g_mock_calls[i].params[j].name, g_mock_calls[i].function_name);
            }
        }
    }
}

void mock_function_call(const char* function_name) {
    TEST_ASSERT_MSG_VOID(g_mock_call_index < g_mock_call_count,
        "Unexpected function call: %s", function_name);
    TEST_ASSERT_MSG_VOID(infra_strcmp(g_mock_calls[g_mock_call_index].function_name, function_name) == 0,
        "Expected function %s but got %s",
        g_mock_calls[g_mock_call_index].function_name, function_name);
    g_mock_calls[g_mock_call_index].called = 1;
    g_current_call = g_mock_call_index;
}

static mock_param_t* find_param(const char* param_name) {
    TEST_ASSERT_MSG_PTR(g_current_call < g_mock_call_count,
        "No function call to find parameter for");
    mock_call_t* call = &g_mock_calls[g_current_call];
    for (size_t i = 0; i < call->param_count; i++) {
        if (!call->params[i].verified && 
            infra_strcmp(call->params[i].name, param_name) == 0) {
            return &call->params[i];
        }
    }
    return NULL;
}

void mock_param_value(const char* param_name, uint64_t value) {
    mock_param_t* param = find_param(param_name);
    TEST_ASSERT_MSG_VOID(param != NULL, "Unexpected parameter value for %s", param_name);
    TEST_ASSERT_MSG_VOID(param->value.value == value,
        "Expected value %lu but got %lu for parameter %s",
        param->value.value, value, param_name);
    param->verified = 1;
}

void mock_param_ptr(const char* param_name, const void* ptr) {
    mock_param_t* param = find_param(param_name);
    TEST_ASSERT_MSG_VOID(param != NULL, "Unexpected parameter pointer for %s", param_name);
    TEST_ASSERT_MSG_VOID(param->value.ptr == ptr,
        "Expected pointer %p but got %p for parameter %s",
        param->value.ptr, ptr, param_name);
    param->verified = 1;
}

void mock_param_str(const char* param_name, const char* str) {
    mock_param_t* param = find_param(param_name);
    TEST_ASSERT_MSG_VOID(param != NULL, "Unexpected parameter string for %s", param_name);
    TEST_ASSERT_MSG_VOID(infra_strcmp(param->value.str, str) == 0,
        "Expected string %s but got %s for parameter %s",
        param->value.str, str, param_name);
    param->verified = 1;
}

void mock_expect_function_call(const char* function_name) {
    TEST_ASSERT_MSG_VOID(g_mock_call_count < MAX_MOCK_CALLS,
        "Too many mock calls");
    mock_call_t* call = &g_mock_calls[g_mock_call_count];
    call->function_name = function_name;
    call->param_count = 0;
    call->called = 0;
    call->returned = 0;
    g_mock_call_count++;
}

void mock_expect_param_value(const char* param_name, uint64_t value) {
    TEST_ASSERT_MSG_VOID(g_mock_call_count > 0,
        "No function call to expect parameter for");
    mock_call_t* call = &g_mock_calls[g_mock_call_count - 1];
    TEST_ASSERT_MSG_VOID(call->param_count < MAX_PARAMS_PER_CALL,
        "Too many parameters for function %s", call->function_name);
    mock_param_t* param = &call->params[call->param_count++];
    param->name = param_name;
    param->value.value = value;
    param->verified = 0;
}

void mock_expect_param_ptr(const char* param_name, const void* ptr) {
    TEST_ASSERT_MSG_VOID(g_mock_call_count > 0,
        "No function call to expect parameter for");
    mock_call_t* call = &g_mock_calls[g_mock_call_count - 1];
    TEST_ASSERT_MSG_VOID(call->param_count < MAX_PARAMS_PER_CALL,
        "Too many parameters for function %s", call->function_name);
    mock_param_t* param = &call->params[call->param_count++];
    param->name = param_name;
    param->value.ptr = (void*)ptr;
    param->verified = 0;
}

void mock_expect_param_str(const char* param_name, const char* str) {
    TEST_ASSERT_MSG_VOID(g_mock_call_count > 0,
        "No function call to expect parameter for");
    mock_call_t* call = &g_mock_calls[g_mock_call_count - 1];
    TEST_ASSERT_MSG_VOID(call->param_count < MAX_PARAMS_PER_CALL,
        "Too many parameters for function %s", call->function_name);
    mock_param_t* param = &call->params[call->param_count++];
    param->name = param_name;
    param->value.str = str;
    param->verified = 0;
}

void* mock_expect_return_ptr(const char* function_name, void* ptr) {
    TEST_ASSERT_MSG_PTR(g_mock_call_count > 0,
        "No function call to expect return value for");
    g_mock_calls[g_mock_call_count - 1].return_val.ptr = ptr;
    return ptr;
}

uint64_t mock_expect_return_value(const char* function_name, uint64_t value) {
    TEST_ASSERT_MSG_INT(g_mock_call_count > 0,
        "No function call to expect return value for");
    g_mock_calls[g_mock_call_count - 1].return_val.value = value;
    return value;
}

uint64_t mock_return_value(const char* function_name) {
    TEST_ASSERT_MSG_INT(g_current_call < g_mock_call_count,
        "Unexpected return value request");
    TEST_ASSERT_MSG_INT(infra_strcmp(g_mock_calls[g_current_call].function_name, function_name) == 0,
        "Expected return value for function %s but got %s",
        g_mock_calls[g_current_call].function_name, function_name);
    g_mock_calls[g_current_call].returned = 1;
    uint64_t value = g_mock_calls[g_current_call].return_val.value;
    g_mock_call_index++;
    return value;
}

void* mock_return_ptr(const char* function_name) {
    TEST_ASSERT_MSG_PTR(g_current_call < g_mock_call_count,
        "Unexpected return pointer request");
    TEST_ASSERT_MSG_PTR(infra_strcmp(g_mock_calls[g_current_call].function_name, function_name) == 0,
        "Expected return pointer for function %s but got %s",
        g_mock_calls[g_current_call].function_name, function_name);
    g_mock_calls[g_current_call].returned = 1;
    void* ptr = g_mock_calls[g_current_call].return_val.ptr;
    g_mock_call_index++;
    return ptr;
} 