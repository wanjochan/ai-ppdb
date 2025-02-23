#include "PolyxScript.h"
#include "internal/infrax/InfraxTest.h"
#include <stdio.h>
#include <string.h>

// Test utilities
static void test_progress_callback(InfraxSize current, InfraxSize total) {
    g_core->printf(g_core, "Progress: %zu/%zu\n", current, total);
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
    INFRAX_TEST_ASSERT(g_core->strstr(g_core, script->error_message, "File does not exist") != NULL);
    
    // Test reading file with invalid permissions
    source = "let result = readFile('/root/test.txt');";
    INFRAX_TEST_ASSERT(script->klass->load_source(script, source) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->had_error);
    INFRAX_TEST_ASSERT(g_core->strstr(g_core, script->error_message, "File is not readable") != NULL);
    
    // Create a test file
    InfraxFile* test_file = g_core->file_open(g_core, "test.txt", "w");
    INFRAX_TEST_ASSERT(test_file != NULL);
    const char* test_content = "Hello, World!";
    g_core->file_write(g_core, test_file, test_content, g_core->strlen(g_core, test_content));
    g_core->file_close(g_core, test_file);
    
    // Test reading valid file
    source = "let result = readFile('test.txt');";
    INFRAX_TEST_ASSERT(script->klass->load_source(script, source) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(!script->had_error);
    INFRAX_TEST_ASSERT(script->last_result != NULL);
    INFRAX_TEST_ASSERT(script->last_result->type == POLYX_VALUE_STRING);
    INFRAX_TEST_ASSERT(g_core->strcmp(g_core, script->last_result->as.string, test_content) == 0);
    
    // Clean up
    g_core->file_remove(g_core, "test.txt");
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
    INFRAX_TEST_ASSERT(g_core->strstr(g_core, script->error_message, "File is not writable") != NULL);
    
    // Test writing valid content
    source = "let result = writeFile('test.txt', 'Hello, World!');";
    INFRAX_TEST_ASSERT(script->klass->load_source(script, source) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(script->klass->run(script) == INFRAX_ERROR_NONE);
    INFRAX_TEST_ASSERT(!script->had_error);
    INFRAX_TEST_ASSERT(script->last_result != NULL);
    INFRAX_TEST_ASSERT(script->last_result->type == POLYX_VALUE_NUMBER);
    INFRAX_TEST_ASSERT(script->last_result->as.number == 13); // Length of "Hello, World!"
    
    // Verify file content
    InfraxFile* test_file = g_core->file_open(g_core, "test.txt", "r");
    INFRAX_TEST_ASSERT(test_file != NULL);
    char buffer[100];
    g_core->file_read(g_core, test_file, buffer, sizeof(buffer));
    g_core->file_close(g_core, test_file);
    INFRAX_TEST_ASSERT(g_core->strcmp(g_core, buffer, "Hello, World!") == 0);
    
    // Clean up
    g_core->file_remove(g_core, "test.txt");
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
    INFRAX_TEST_ASSERT(g_core->strstr(g_core, script->error_message, "timeout") != NULL);
    
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
    InfraxFile* test_file = g_core->file_open(g_core, "large_test.txt", "w");
    INFRAX_TEST_ASSERT(test_file != NULL);
    for (int i = 0; i < 1000; i++) {
        g_core->file_write(g_core, test_file, "Line ", 5);
        g_core->file_write(g_core, test_file, g_core->itoa(g_core, i), g_core->strlen(g_core, g_core->itoa(g_core, i)));
        g_core->file_write(g_core, test_file, ": This is a test line for progress callback testing.\n", 37);
    }
    g_core->file_close(g_core, test_file);
    
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
    g_core->file_remove(g_core, "large_test.txt");
    g_core->file_remove(g_core, "output.txt");
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