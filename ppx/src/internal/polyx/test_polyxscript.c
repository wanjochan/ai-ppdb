#include "PolyxScript.h"
#include "internal/infrax/InfraxMemory.h"

// Test framework
static InfraxSize total_tests = 0;
static InfraxSize passed_tests = 0;

static void assert_true(const char* test_name, InfraxBool condition) {
    total_tests++;
    if (condition) {
        passed_tests++;
        g_core->printf(g_core, "[PASS] %s\n", test_name);
    } else {
        g_core->printf(g_core, "[FAIL] %s\n", test_name);
    }
}

static void assert_false(const char* test_name, InfraxBool condition) {
    assert_true(test_name, !condition);
}

static void assert_number_eq(const char* test_name, double expected, double actual) {
    total_tests++;
    if (expected == actual) {
        passed_tests++;
        g_core->printf(g_core, "[PASS] %s\n", test_name);
    } else {
        g_core->printf(g_core, "[FAIL] %s (expected: %f, got: %f)\n", test_name, expected, actual);
    }
}

static void assert_string_eq(const char* test_name, const char* expected, const char* actual) {
    total_tests++;
    if (g_core->strcmp(g_core, expected, actual) == 0) {
        passed_tests++;
        g_core->printf(g_core, "[PASS] %s\n", test_name);
    } else {
        g_core->printf(g_core, "[FAIL] %s (expected: \"%s\", got: \"%s\")\n", test_name, expected, actual);
    }
}

// Test cases
static void test_number_literals(void) {
    PolyxScript* script = PolyxScriptClass.new();
    
    // Test simple number
    script->klass->load_source(script, "42");
    InfraxError error = script->klass->run(script);
    assert_true("Run simple number", error.code == 0);
    assert_true("Result is number", script->last_result->type == POLYX_VALUE_NUMBER);
    assert_number_eq("Number value", 42.0, script->last_result->as.number);
    
    // Test negative number
    script->klass->load_source(script, "-42");
    error = script->klass->run(script);
    assert_true("Run negative number", error.code == 0);
    assert_true("Result is number", script->last_result->type == POLYX_VALUE_NUMBER);
    assert_number_eq("Negative number value", -42.0, script->last_result->as.number);
    
    // Test decimal number
    script->klass->load_source(script, "3.14");
    error = script->klass->run(script);
    assert_true("Run decimal number", error.code == 0);
    assert_true("Result is number", script->last_result->type == POLYX_VALUE_NUMBER);
    assert_number_eq("Decimal number value", 3.14, script->last_result->as.number);
    
    script->klass->free(script);
}

static void test_string_literals(void) {
    PolyxScript* script = PolyxScriptClass.new();
    
    // Test empty string
    script->klass->load_source(script, "\"\"");
    InfraxError error = script->klass->run(script);
    assert_true("Run empty string", error.code == 0);
    assert_true("Result is string", script->last_result->type == POLYX_VALUE_STRING);
    assert_string_eq("Empty string value", "", script->last_result->as.string);
    
    // Test simple string
    script->klass->load_source(script, "\"Hello, World!\"");
    error = script->klass->run(script);
    assert_true("Run simple string", error.code == 0);
    assert_true("Result is string", script->last_result->type == POLYX_VALUE_STRING);
    assert_string_eq("String value", "Hello, World!", script->last_result->as.string);
    
    script->klass->free(script);
}

static void test_arithmetic_operations(void) {
    PolyxScript* script = PolyxScriptClass.new();
    
    // Test addition
    script->klass->load_source(script, "2 + 3");
    InfraxError error = script->klass->run(script);
    assert_true("Run addition", error.code == 0);
    assert_true("Result is number", script->last_result->type == POLYX_VALUE_NUMBER);
    assert_number_eq("Addition result", 5.0, script->last_result->as.number);
    
    // Test subtraction
    script->klass->load_source(script, "5 - 3");
    error = script->klass->run(script);
    assert_true("Run subtraction", error.code == 0);
    assert_true("Result is number", script->last_result->type == POLYX_VALUE_NUMBER);
    assert_number_eq("Subtraction result", 2.0, script->last_result->as.number);
    
    // Test multiplication
    script->klass->load_source(script, "4 * 3");
    error = script->klass->run(script);
    assert_true("Run multiplication", error.code == 0);
    assert_true("Result is number", script->last_result->type == POLYX_VALUE_NUMBER);
    assert_number_eq("Multiplication result", 12.0, script->last_result->as.number);
    
    // Test division
    script->klass->load_source(script, "10 / 2");
    error = script->klass->run(script);
    assert_true("Run division", error.code == 0);
    assert_true("Result is number", script->last_result->type == POLYX_VALUE_NUMBER);
    assert_number_eq("Division result", 5.0, script->last_result->as.number);
    
    // Test complex expression
    script->klass->load_source(script, "2 + 3 * 4 - 6 / 2");
    error = script->klass->run(script);
    assert_true("Run complex expression", error.code == 0);
    assert_true("Result is number", script->last_result->type == POLYX_VALUE_NUMBER);
    assert_number_eq("Complex expression result", 11.0, script->last_result->as.number);
    
    script->klass->free(script);
}

static void test_variables(void) {
    PolyxScript* script = PolyxScriptClass.new();
    
    // Test variable declaration
    script->klass->load_source(script, "let x = 42");
    InfraxError error = script->klass->run(script);
    assert_true("Run variable declaration", error.code == 0);
    
    // Test variable reference
    script->klass->load_source(script, "x");
    error = script->klass->run(script);
    assert_true("Run variable reference", error.code == 0);
    assert_true("Result is number", script->last_result->type == POLYX_VALUE_NUMBER);
    assert_number_eq("Variable value", 42.0, script->last_result->as.number);
    
    // Test variable assignment
    script->klass->load_source(script, "x = 24");
    error = script->klass->run(script);
    assert_true("Run variable assignment", error.code == 0);
    
    script->klass->load_source(script, "x");
    error = script->klass->run(script);
    assert_true("Run variable reference after assignment", error.code == 0);
    assert_true("Result is number", script->last_result->type == POLYX_VALUE_NUMBER);
    assert_number_eq("Variable value after assignment", 24.0, script->last_result->as.number);
    
    script->klass->free(script);
}

static void test_control_flow(void) {
    PolyxScript* script = PolyxScriptClass.new();
    
    // Test if statement
    script->klass->load_source(script,
        "let x = 10\n"
        "if (x > 5) {\n"
        "    x = 1\n"
        "} else {\n"
        "    x = 2\n"
        "}\n"
        "x"
    );
    InfraxError error = script->klass->run(script);
    assert_true("Run if statement", error.code == 0);
    assert_true("Result is number", script->last_result->type == POLYX_VALUE_NUMBER);
    assert_number_eq("If statement result", 1.0, script->last_result->as.number);
    
    // Test while loop
    script->klass->load_source(script,
        "let x = 0\n"
        "let i = 0\n"
        "while (i < 5) {\n"
        "    x = x + i\n"
        "    i = i + 1\n"
        "}\n"
        "x"
    );
    error = script->klass->run(script);
    assert_true("Run while loop", error.code == 0);
    assert_true("Result is number", script->last_result->type == POLYX_VALUE_NUMBER);
    assert_number_eq("While loop result", 10.0, script->last_result->as.number);
    
    script->klass->free(script);
}

static void test_builtin_functions(void) {
    PolyxScript* script = PolyxScriptClass.new();
    
    // Test toString
    script->klass->load_source(script, "toString(42)");
    InfraxError error = script->klass->run(script);
    assert_true("Run toString", error.code == 0);
    assert_true("Result is string", script->last_result->type == POLYX_VALUE_STRING);
    assert_string_eq("toString result", "42", script->last_result->as.string);
    
    // Test toNumber
    script->klass->load_source(script, "toNumber(\"42\")");
    error = script->klass->run(script);
    assert_true("Run toNumber", error.code == 0);
    assert_true("Result is number", script->last_result->type == POLYX_VALUE_NUMBER);
    assert_number_eq("toNumber result", 42.0, script->last_result->as.number);
    
    // Test array operations
    script->klass->load_source(script,
        "let arr = []\n"
        "arrayPush(arr, 1, 2, 3)\n"
        "arrayLength(arr)"
    );
    error = script->klass->run(script);
    assert_true("Run array operations", error.code == 0);
    assert_true("Result is number", script->last_result->type == POLYX_VALUE_NUMBER);
    assert_number_eq("Array length", 3.0, script->last_result->as.number);
    
    // Test object operations
    script->klass->load_source(script,
        "let obj = {}\n"
        "objectSet(obj, \"key\", 42)\n"
        "objectGet(obj, \"key\")"
    );
    error = script->klass->run(script);
    assert_true("Run object operations", error.code == 0);
    assert_true("Result is number", script->last_result->type == POLYX_VALUE_NUMBER);
    assert_number_eq("Object value", 42.0, script->last_result->as.number);
    
    script->klass->free(script);
}

static void test_promises(void) {
    PolyxScript* script = PolyxScriptClass.new();
    
    // Test promise creation
    PolyxValue* promise = script->klass->create_promise(script);
    assert_true("Create promise", promise != NULL);
    assert_true("Promise type", promise->type == POLYX_VALUE_PROMISE);
    assert_true("Promise state", promise->as.promise.state == POLYX_ASYNC_PENDING);
    
    // Test promise resolution
    PolyxValue* value = script_create_number_value(42);
    script->klass->resolve_promise(script, promise, value);
    assert_true("Promise resolved", promise->as.promise.state == POLYX_ASYNC_COMPLETED);
    assert_true("Promise result type", promise->as.promise.result->type == POLYX_VALUE_NUMBER);
    assert_number_eq("Promise result value", 42.0, promise->as.promise.result->as.number);
    
    // Test promise rejection
    PolyxValue* promise2 = script->klass->create_promise(script);
    script->klass->reject_promise(script, promise2, "Test error");
    assert_true("Promise rejected", promise2->as.promise.state == POLYX_ASYNC_ERROR);
    assert_true("Promise error type", promise2->as.promise.result->type == POLYX_VALUE_STRING);
    assert_string_eq("Promise error message", "Test error", promise2->as.promise.result->as.string);
    
    script_free_value(promise);
    script_free_value(promise2);
    script->klass->free(script);
}

static void test_async_sleep(void) {
    PolyxScript* script = PolyxScriptClass.new();
    
    // Test sleep function
    PolyxValue* args[] = { script_create_number_value(1000) };  // 1 second
    PolyxValue* promise = script->klass->async_sleep(script, args, 1);
    assert_true("Create sleep promise", promise != NULL);
    assert_true("Sleep promise type", promise->type == POLYX_VALUE_PROMISE);
    assert_true("Sleep promise state", promise->as.promise.state == POLYX_ASYNC_PENDING);
    
    // Update async operations
    script->klass->update_async(script);
    assert_true("Sleep promise completed", promise->as.promise.state == POLYX_ASYNC_COMPLETED);
    assert_true("Sleep result type", promise->as.promise.result->type == POLYX_VALUE_NULL);
    
    script_free_value(args[0]);
    script_free_value(promise);
    script->klass->free(script);
}

static void test_async_read_file(void) {
    PolyxScript* script = PolyxScriptClass.new();
    
    // Test readFile function
    PolyxValue* args[] = { script_create_string_value("test.txt") };
    PolyxValue* promise = script->klass->async_read_file(script, args, 1);
    assert_true("Create readFile promise", promise != NULL);
    assert_true("ReadFile promise type", promise->type == POLYX_VALUE_PROMISE);
    assert_true("ReadFile promise state", promise->as.promise.state == POLYX_ASYNC_PENDING);
    
    // Update async operations
    script->klass->update_async(script);
    assert_true("ReadFile promise completed", promise->as.promise.state == POLYX_ASYNC_COMPLETED);
    assert_true("ReadFile result type", promise->as.promise.result->type == POLYX_VALUE_STRING);
    
    script_free_value(args[0]);
    script_free_value(promise);
    script->klass->free(script);
}

static void test_async_error_handling(void) {
    PolyxScript* script = PolyxScriptClass.new();
    
    // Test invalid sleep argument
    PolyxValue* args[] = { script_create_string_value("invalid") };
    PolyxValue* promise = script->klass->async_sleep(script, args, 1);
    assert_true("Sleep with invalid argument", promise == NULL);
    assert_true("Error flag set", script->had_error);
    assert_string_eq("Error message", "sleep() argument must be a number", script->error_message);
    
    // Test readFile with invalid argument count
    promise = script->klass->async_read_file(script, NULL, 0);
    assert_true("ReadFile with no arguments", promise == NULL);
    assert_true("Error flag set", script->had_error);
    assert_string_eq("Error message", "readFile() requires exactly one argument", script->error_message);
    
    script_free_value(args[0]);
    script->klass->free(script);
}

// Main test runner
int main(void) {
    g_core->printf(g_core, "Running PolyxScript tests...\n\n");
    
    test_number_literals();
    test_string_literals();
    test_arithmetic_operations();
    test_variables();
    test_control_flow();
    test_builtin_functions();
    test_promises();
    test_async_sleep();
    test_async_read_file();
    test_async_error_handling();
    
    g_core->printf(g_core, "\nTest summary: %zu/%zu tests passed\n",
                  passed_tests, total_tests);
    
    return passed_tests == total_tests ? 0 : 1;
} 