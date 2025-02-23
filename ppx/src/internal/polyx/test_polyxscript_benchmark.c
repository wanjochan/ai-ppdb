#include "PolyxScript.h"
#include "internal/infrax/InfraxTest.h"

// Benchmark utilities
typedef struct {
    const char* name;
    InfraxSize iterations;
    InfraxSize memory_start;
    InfraxSize memory_end;
    double time_start;
    double time_end;
} BenchmarkResult;

static void benchmark_begin(BenchmarkResult* result, const char* name, InfraxSize iterations) {
    result->name = name;
    result->iterations = iterations;
    result->memory_start = g_core->get_memory_usage(g_core);
    result->time_start = g_core->time_monotonic_ms(g_core) / 1000.0;
}

static void benchmark_end(BenchmarkResult* result) {
    result->time_end = g_core->time_monotonic_ms(g_core) / 1000.0;
    result->memory_end = g_core->get_memory_usage(g_core);
    
    double time_elapsed = result->time_end - result->time_start;
    InfraxSize memory_used = result->memory_end - result->memory_start;
    double ops_per_sec = result->iterations / time_elapsed;
    
    g_core->printf(g_core, "\nBenchmark: %s\n", result->name);
    g_core->printf(g_core, "  Iterations: %zu\n", result->iterations);
    g_core->printf(g_core, "  Time: %.3f seconds\n", time_elapsed);
    g_core->printf(g_core, "  Memory: %zu bytes\n", memory_used);
    g_core->printf(g_core, "  Operations/sec: %.2f\n", ops_per_sec);
}

// Basic operation benchmarks
static void benchmark_number_literals(void) {
    const InfraxSize iterations = 100000;
    BenchmarkResult result;
    benchmark_begin(&result, "Number Literals", iterations);
    
    PolyxScript* script = PolyxScriptClass.new();
    INFRAX_TEST_ASSERT(script != NULL);
    
    for (InfraxSize i = 0; i < iterations; i++) {
        script->klass->load_source(script, "42");
        INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    }
    
    script->klass->free(script);
    benchmark_end(&result);
}

static void benchmark_string_literals(void) {
    const InfraxSize iterations = 100000;
    BenchmarkResult result;
    benchmark_begin(&result, "String Literals", iterations);
    
    PolyxScript* script = PolyxScriptClass.new();
    INFRAX_TEST_ASSERT(script != NULL);
    
    for (InfraxSize i = 0; i < iterations; i++) {
        script->klass->load_source(script, "\"Hello, World!\"");
        INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    }
    
    script->klass->free(script);
    benchmark_end(&result);
}

static void benchmark_arithmetic_operations(void) {
    const InfraxSize iterations = 100000;
    BenchmarkResult result;
    benchmark_begin(&result, "Arithmetic Operations", iterations);
    
    PolyxScript* script = PolyxScriptClass.new();
    INFRAX_TEST_ASSERT(script != NULL);
    
    for (InfraxSize i = 0; i < iterations; i++) {
        script->klass->load_source(script, "2 + 3 * 4 - 6 / 2");
        INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    }
    
    script->klass->free(script);
    benchmark_end(&result);
}

static void benchmark_variable_operations(void) {
    const InfraxSize iterations = 100000;
    BenchmarkResult result;
    benchmark_begin(&result, "Variable Operations", iterations);
    
    PolyxScript* script = PolyxScriptClass.new();
    INFRAX_TEST_ASSERT(script != NULL);
    
    for (InfraxSize i = 0; i < iterations; i++) {
        script->klass->load_source(script,
            "let x = 42\n"
            "x = x + 1\n"
            "x"
        );
        INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    }
    
    script->klass->free(script);
    benchmark_end(&result);
}

static void benchmark_function_calls(void) {
    const InfraxSize iterations = 100000;
    BenchmarkResult result;
    benchmark_begin(&result, "Function Calls", iterations);
    
    PolyxScript* script = PolyxScriptClass.new();
    INFRAX_TEST_ASSERT(script != NULL);
    
    for (InfraxSize i = 0; i < iterations; i++) {
        script->klass->load_source(script,
            "toString(42)\n"
            "toNumber(\"42\")"
        );
        INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    }
    
    script->klass->free(script);
    benchmark_end(&result);
}

static void benchmark_array_operations(void) {
    const InfraxSize iterations = 10000;
    BenchmarkResult result;
    benchmark_begin(&result, "Array Operations", iterations);
    
    PolyxScript* script = PolyxScriptClass.new();
    INFRAX_TEST_ASSERT(script != NULL);
    
    for (InfraxSize i = 0; i < iterations; i++) {
        script->klass->load_source(script,
            "let arr = []\n"
            "arrayPush(arr, 1, 2, 3, 4, 5)\n"
            "arrayPop(arr)\n"
            "arrayLength(arr)"
        );
        INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    }
    
    script->klass->free(script);
    benchmark_end(&result);
}

static void benchmark_object_operations(void) {
    const InfraxSize iterations = 10000;
    BenchmarkResult result;
    benchmark_begin(&result, "Object Operations", iterations);
    
    PolyxScript* script = PolyxScriptClass.new();
    INFRAX_TEST_ASSERT(script != NULL);
    
    for (InfraxSize i = 0; i < iterations; i++) {
        script->klass->load_source(script,
            "let obj = {}\n"
            "objectSet(obj, \"key1\", 1)\n"
            "objectSet(obj, \"key2\", 2)\n"
            "objectGet(obj, \"key1\")\n"
            "objectGet(obj, \"key2\")"
        );
        INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    }
    
    script->klass->free(script);
    benchmark_end(&result);
}

int main(void) {
    g_core->printf(g_core, "PolyxScript Benchmark Tests\n");
    g_core->printf(g_core, "==========================\n");
    
    // Basic operation benchmarks
    benchmark_number_literals();
    benchmark_string_literals();
    benchmark_arithmetic_operations();
    benchmark_variable_operations();
    benchmark_function_calls();
    
    // Complex operation benchmarks
    benchmark_array_operations();
    benchmark_object_operations();
    
    g_core->printf(g_core, "\nAll benchmarks completed.\n");
    return 0;
} 