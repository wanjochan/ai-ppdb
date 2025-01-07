#include "test_framework.h"
#include "ppdb/ast.h"

/* Test cases */
static int test_parse_number(void) {
    const char *input = "42.5";
    ast_node_t *node = ast_expr(input);
    TEST_ASSERT_NOT_NULL(node);
    ast_free(node);
    return 0;
}

static int test_parse_symbol(void) {
    const char *input = "variable_name123";
    ast_node_t *node = ast_expr(input);
    TEST_ASSERT_NOT_NULL(node);
    ast_free(node);
    return 0;
}

static int test_parse_assign_equals(void) {
    const char *input = "=(x, 42)";
    ast_node_t *node = ast_expr(input);
    TEST_ASSERT_NOT_NULL(node);
    ast_free(node);
    return 0;
}

static int test_parse_assign_local(void) {
    const char *input = "local(x, 42)";
    ast_node_t *node = ast_expr(input);
    TEST_ASSERT_NOT_NULL(node);
    ast_free(node);
    return 0;
}

/* Test suite */
static const test_case_t test_cases[] = {
    {
        .name = "test_parse_number",
        .fn = test_parse_number,
        .timeout_seconds = 5
    },
    {
        .name = "test_parse_symbol",
        .fn = test_parse_symbol,
        .timeout_seconds = 5
    },
    {
        .name = "test_parse_assign_equals",
        .fn = test_parse_assign_equals,
        .timeout_seconds = 5
    },
    {
        .name = "test_parse_assign_local",
        .fn = test_parse_assign_local,
        .timeout_seconds = 5
    }
};

static const test_suite_t test_suite = {
    .name = "AST Parser Tests",
    .cases = test_cases,
    .num_cases = sizeof(test_cases) / sizeof(test_cases[0])
};

/* Test runner */
int main(void) {
    TEST_INIT();
    int result = run_test_suite(&test_suite);
    TEST_CLEANUP();
    return result;
} 