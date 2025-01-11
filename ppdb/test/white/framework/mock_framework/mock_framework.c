#include "mock_framework.h"
#include "internal/infra/infra.h"
#include "cosmopolitan.h"

// Global state
static mock_expectation_t* g_expectations = NULL;
static char g_last_error[256] = {0};

// Set last error message
static void set_last_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    infra_vsnprintf(g_last_error, sizeof(g_last_error), format, args);
    va_end(args);
}

// Initialize mock framework
mock_error_t mock_framework_init(void) {
    g_expectations = NULL;
    g_last_error[0] = '\0';
    return MOCK_OK;
}

// Cleanup mock framework
void mock_framework_cleanup(void) {
    mock_expectation_t* current = g_expectations;
    while (current) {
        mock_expectation_t* next = current->next;
        infra_free(current);
        current = next;
    }
    g_expectations = NULL;
}

// Register a new mock expectation
mock_expectation_t* mock_register_expectation(const char* func_name) {
    if (!func_name) {
        set_last_error("Invalid function name");
        return NULL;
    }

    // Find existing expectation
    mock_expectation_t* exp = g_expectations;
    while (exp) {
        if (infra_strcmp(exp->func_name, func_name) == 0) {
            return exp;
        }
        exp = exp->next;
    }

    // Create new expectation
    exp = infra_malloc(sizeof(mock_expectation_t));
    if (!exp) {
        set_last_error("Failed to allocate memory for mock expectation");
        return NULL;
    }

    exp->func_name = func_name;
    exp->expected_calls = 1;  // Default to 1 expected call
    exp->actual_calls = 0;
    exp->return_value = NULL;
    exp->next = g_expectations;
    g_expectations = exp;

    return exp;
}

// Set expected call count
void mock_expect_times(mock_expectation_t* exp, int count) {
    if (exp) {
        exp->expected_calls = count;
    }
}

// Set return value
void mock_will_return(mock_expectation_t* exp, void* value) {
    if (exp) {
        exp->return_value = value;
    }
}

// Verify all expectations
mock_error_t mock_verify_all_expectations(void) {
    mock_expectation_t* exp = g_expectations;
    while (exp) {
        if (exp->actual_calls != exp->expected_calls) {
            set_last_error("Mock expectation failed for %s: expected %d calls, got %d",
                          exp->func_name, exp->expected_calls, exp->actual_calls);
            return MOCK_ERROR_EXPECTATION_FAILED;
        }
        exp = exp->next;
    }
    return MOCK_OK;
}

// Get last error message
const char* mock_get_last_error(void) {
    return g_last_error;
} 