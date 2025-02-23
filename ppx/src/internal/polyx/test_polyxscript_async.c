#include "PolyxScript.h"
#include "internal/infrax/InfraxTest.h"
#include <stdio.h>
#include <string.h>

// Test utilities
static void test_progress_callback(InfraxSize current, InfraxSize total) {
    printf("Progress: %zu/%zu\n", current, total);
}

// File operation tests
static void test_async_file_read(void) {
    PolyxScript* script = PolyxScriptClass.new();
    INFRAX_TEST_ASSERT(script != NULL);
    
    // Test reading non-existent file
    const char* source = "let result = readFile('non_existent.txt');";
    INFRAX_TEST_ASSERT(script->klass->load_source(script, source) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->had_error);
    INFRAX_TEST_ASSERT(strstr(script->error_message, "File does not exist") != NULL);
    
    // Test reading file with invalid permissions
    source = "let result = readFile('/root/test.txt');";
    INFRAX_TEST_ASSERT(script->klass->load_source(script, source) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->had_error);
    INFRAX_TEST_ASSERT(strstr(script->error_message, "File is not readable") != NULL);
    
    // Create a test file
    FILE* test_file = fopen("test.txt", "w");
    INFRAX_TEST_ASSERT(test_file != NULL);
    const char* test_content = "Hello, World!";
    fprintf(test_file, "%s", test_content);
    fclose(test_file);
    
    // Test reading valid file
    source = "let result = readFile('test.txt');";
    INFRAX_TEST_ASSERT(script->klass->load_source(script, source) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(!script->had_error);
    INFRAX_TEST_ASSERT(script->last_result != NULL);
    INFRAX_TEST_ASSERT(script->last_result->type == POLYX_VALUE_STRING);
    INFRAX_TEST_ASSERT(strcmp(script->last_result->as.string, test_content) == 0);
    
    // Clean up
    remove("test.txt");
    script->klass->free(script);
}

static void test_async_file_write(void) {
    PolyxScript* script = PolyxScriptClass.new();
    INFRAX_TEST_ASSERT(script != NULL);
    
    // Test writing to invalid path
    const char* source = "let result = writeFile('/root/test.txt', 'Hello');";
    INFRAX_TEST_ASSERT(script->klass->load_source(script, source) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->had_error);
    INFRAX_TEST_ASSERT(strstr(script->error_message, "File is not writable") != NULL);
    
    // Test writing valid content
    source = "let result = writeFile('test.txt', 'Hello, World!');";
    INFRAX_TEST_ASSERT(script->klass->load_source(script, source) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(!script->had_error);
    INFRAX_TEST_ASSERT(script->last_result != NULL);
    INFRAX_TEST_ASSERT(script->last_result->type == POLYX_VALUE_NUMBER);
    INFRAX_TEST_ASSERT(script->last_result->as.number == 13); // Length of "Hello, World!"
    
    // Verify file content
    FILE* test_file = fopen("test.txt", "r");
    INFRAX_TEST_ASSERT(test_file != NULL);
    char buffer[100];
    fgets(buffer, sizeof(buffer), test_file);
    fclose(test_file);
    INFRAX_TEST_ASSERT(strcmp(buffer, "Hello, World!") == 0);
    
    // Clean up
    remove("test.txt");
    script->klass->free(script);
}

// Network request tests
static void test_async_http_get(void) {
    PolyxScript* script = PolyxScriptClass.new();
    INFRAX_TEST_ASSERT(script != NULL);
    
    // Test invalid URL
    const char* source = "let result = httpGet('invalid_url');";
    INFRAX_TEST_ASSERT(script->klass->load_source(script, source) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->had_error);
    
    // Test valid GET request
    source = "let result = httpGet('https://api.example.com/test');";
    INFRAX_TEST_ASSERT(script->klass->load_source(script, source) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(!script->had_error);
    INFRAX_TEST_ASSERT(script->last_result != NULL);
    INFRAX_TEST_ASSERT(script->last_result->type == POLYX_VALUE_OBJECT);
    
    // Clean up
    script->klass->free(script);
}

static void test_async_http_post(void) {
    PolyxScript* script = PolyxScriptClass.new();
    INFRAX_TEST_ASSERT(script != NULL);
    
    // Test POST request with body
    const char* source = 
        "let headers = {'Content-Type': 'application/json'};\n"
        "let body = '{\"name\": \"test\"}';\n"
        "let result = httpPost('https://api.example.com/test', headers, body);";
    
    INFRAX_TEST_ASSERT(script->klass->load_source(script, source) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(!script->had_error);
    INFRAX_TEST_ASSERT(script->last_result != NULL);
    INFRAX_TEST_ASSERT(script->last_result->type == POLYX_VALUE_OBJECT);
    
    // Clean up
    script->klass->free(script);
}

// Error handling tests
static void test_async_error_handling(void) {
    PolyxScript* script = PolyxScriptClass.new();
    INFRAX_TEST_ASSERT(script != NULL);
    
    // Test timeout handling
    const char* source = 
        "let result = httpGet('https://api.example.com/test', null, null, 1);"; // 1ms timeout
    
    INFRAX_TEST_ASSERT(script->klass->load_source(script, source) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->had_error);
    INFRAX_TEST_ASSERT(strstr(script->error_message, "timeout") != NULL);
    
    // Test network error handling
    source = "let result = httpGet('https://invalid.domain.test');";
    INFRAX_TEST_ASSERT(script->klass->load_source(script, source) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->had_error);
    
    // Clean up
    script->klass->free(script);
}

// Progress callback tests
static void test_async_progress_callbacks(void) {
    PolyxScript* script = PolyxScriptClass.new();
    INFRAX_TEST_ASSERT(script != NULL);
    
    // Create a large test file
    FILE* test_file = fopen("large_test.txt", "w");
    INFRAX_TEST_ASSERT(test_file != NULL);
    for (int i = 0; i < 1000; i++) {
        fprintf(test_file, "Line %d: This is a test line for progress callback testing.\n", i);
    }
    fclose(test_file);
    
    // Test file read with progress
    const char* source = "let result = readFile('large_test.txt');";
    INFRAX_TEST_ASSERT(script->klass->load_source(script, source) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(!script->had_error);
    
    // Test file write with progress
    source = "let content = 'Large content...'; let result = writeFile('output.txt', content);";
    INFRAX_TEST_ASSERT(script->klass->load_source(script, source) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(!script->had_error);
    
    // Clean up
    remove("large_test.txt");
    remove("output.txt");
    script->klass->free(script);
}

int main(void) {
    INFRAX_TEST_BEGIN("PolyxScript Async Tests");
    
    // File operation tests
    INFRAX_TEST_RUN(test_async_file_read);
    INFRAX_TEST_RUN(test_async_file_write);
    
    // Network request tests
    INFRAX_TEST_RUN(test_async_http_get);
    INFRAX_TEST_RUN(test_async_http_post);
    
    // Error handling tests
    INFRAX_TEST_RUN(test_async_error_handling);
    
    // Progress callback tests
    INFRAX_TEST_RUN(test_async_progress_callbacks);
    
    INFRAX_TEST_END();
    return 0;
} 