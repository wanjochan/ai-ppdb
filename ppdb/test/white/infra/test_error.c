#include <cosmopolitan.h>
#include "ppdb/ppdb.h"
#include "ppdb/internal.h"
#include "test_framework.h"

static void test_error_codes() {
    // Test basic error codes
    ASSERT_EQ(PPDB_OK, 0);
    ASSERT_NE(PPDB_ERROR_OOM, PPDB_OK);
    ASSERT_NE(PPDB_ERROR_IO, PPDB_OK);
    ASSERT_NE(PPDB_ERROR_INVALID, PPDB_OK);
    
    // Test error code uniqueness
    ASSERT_NE(PPDB_ERROR_OOM, PPDB_ERROR_IO);
    ASSERT_NE(PPDB_ERROR_OOM, PPDB_ERROR_INVALID);
    ASSERT_NE(PPDB_ERROR_IO, PPDB_ERROR_INVALID);
}

static void test_error_strings() {
    // Test error string retrieval
    const char* ok_str = ppdb_error_string(PPDB_OK);
    const char* oom_str = ppdb_error_string(PPDB_ERROR_OOM);
    const char* io_str = ppdb_error_string(PPDB_ERROR_IO);
    const char* invalid_str = ppdb_error_string(PPDB_ERROR_INVALID);
    
    // Verify strings are not NULL
    ASSERT_NOT_NULL(ok_str);
    ASSERT_NOT_NULL(oom_str);
    ASSERT_NOT_NULL(io_str);
    ASSERT_NOT_NULL(invalid_str);
    
    // Verify strings are unique
    ASSERT_NE(strcmp(ok_str, oom_str), 0);
    ASSERT_NE(strcmp(ok_str, io_str), 0);
    ASSERT_NE(strcmp(ok_str, invalid_str), 0);
    ASSERT_NE(strcmp(oom_str, io_str), 0);
    ASSERT_NE(strcmp(oom_str, invalid_str), 0);
    ASSERT_NE(strcmp(io_str, invalid_str), 0);
}

static void test_error_propagation() {
    ppdb_error_t err;
    
    // Test function that should succeed
    err = ppdb_sync_create(NULL, 0);
    ASSERT_OK(err);
    
    // Test function that should fail with INVALID
    err = ppdb_sync_lock(NULL);
    ASSERT_EQ(err, PPDB_ERROR_INVALID);
    
    // Test function that should fail with OOM
    void* ptr = PPDB_ALIGNED_ALLOC((size_t)-1);  // Too large
    ASSERT_NULL(ptr);
}

static void test_error_recovery() {
    ppdb_sync_t* sync = NULL;
    ppdb_error_t err;
    
    // Test recovery from invalid parameter
    err = ppdb_sync_create(&sync, 999);  // Invalid type
    ASSERT_NE(err, PPDB_OK);
    ASSERT_NULL(sync);  // Should not be modified on error
    
    // Test successful operation after error
    err = ppdb_sync_create(&sync, 0);  // Valid type
    ASSERT_OK(err);
    ASSERT_NOT_NULL(sync);
    
    // Cleanup
    ppdb_sync_destroy(sync);
}

int main() {
    TEST_SUITE_BEGIN("Error Tests");
    
    TEST_RUN(test_error_codes);
    TEST_RUN(test_error_strings);
    TEST_RUN(test_error_propagation);
    TEST_RUN(test_error_recovery);
    
    TEST_SUITE_END();
    return 0;
} 