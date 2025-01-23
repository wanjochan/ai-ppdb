/*
 * infra_error.c - Error handling implementation
 */

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"

// 预期错误状态
static struct {
    bool has_expected;
    infra_error_t expected_error;
} g_error_state = {false, INFRA_OK};

const char* infra_error_string(infra_error_t err) {
    switch (err) {
        case INFRA_OK:
            return "Success";
        case INFRA_ERROR_INVALID:
            return "Invalid parameter";
        case INFRA_ERROR_NO_MEMORY:
            return "No memory";
        case INFRA_ERROR_TIMEOUT:
            return "Timeout";
        case INFRA_ERROR_BUSY:
            return "Resource busy";
        case INFRA_ERROR_NOT_FOUND:
            return "Not found";
        case INFRA_ERROR_EXISTS:
            return "Already exists";
        case INFRA_ERROR_IO:
            return "I/O error";
        default:
            return "Unknown error";
    }
}

void infra_set_expected_error(infra_error_t err) {
    g_error_state.has_expected = true;
    g_error_state.expected_error = err;
}

void infra_clear_expected_error(void) {
    g_error_state.has_expected = false;
    g_error_state.expected_error = INFRA_OK;
}

bool infra_is_expected_error(infra_error_t err) {
    return g_error_state.has_expected && g_error_state.expected_error == err;
} 
