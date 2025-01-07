#include <cosmopolitan.h>
#include "internal/base.h"

// Test memory pool basic operations
static void test_memory_pool_basic(void) {
    ppdb_error_t err;
    ppdb_base_memory_pool_t* pool = NULL;
    ppdb_base_memory_stats_t stats;

    // Create memory pool
    err = ppdb_base_memory_pool_create(&pool, 1024 * 1024); // 1MB pool
    assert(err == PPDB_OK);
    assert(pool != NULL);

    // Check initial stats
    ppdb_base_memory_pool_get_stats(pool, &stats);
    assert(stats.total_allocated == 0);
    assert(stats.total_freed == 0);
    assert(stats.current_usage == 0);
    assert(stats.peak_usage == 0);

    // Cleanup
    ppdb_base_memory_pool_destroy(pool);
}

// Test memory allocation and deallocation
static void test_memory_operations(void) {
    ppdb_error_t err;
    ppdb_base_memory_pool_t* pool = NULL;
    ppdb_base_memory_stats_t stats;
    void* ptr1 = NULL;
    void* ptr2 = NULL;
    const size_t size1 = 1024;
    const size_t size2 = 2048;

    // Create memory pool
    err = ppdb_base_memory_pool_create(&pool, 1024 * 1024);
    assert(err == PPDB_OK);

    // Allocate memory
    err = ppdb_base_memory_pool_alloc(pool, size1, &ptr1);
    assert(err == PPDB_OK);
    assert(ptr1 != NULL);

    // Check stats after first allocation
    ppdb_base_memory_pool_get_stats(pool, &stats);
    assert(stats.total_allocated == size1);
    assert(stats.current_usage == size1);
    assert(stats.peak_usage == size1);

    // Allocate more memory
    err = ppdb_base_memory_pool_alloc(pool, size2, &ptr2);
    assert(err == PPDB_OK);
    assert(ptr2 != NULL);

    // Check stats after second allocation
    ppdb_base_memory_pool_get_stats(pool, &stats);
    assert(stats.total_allocated == size1 + size2);
    assert(stats.current_usage == size1 + size2);
    assert(stats.peak_usage == size1 + size2);

    // Free first allocation
    err = ppdb_base_memory_pool_free(pool, ptr1);
    assert(err == PPDB_OK);

    // Check stats after free
    ppdb_base_memory_pool_get_stats(pool, &stats);
    assert(stats.total_freed == size1);
    assert(stats.current_usage == size2);
    assert(stats.peak_usage == size1 + size2);

    // Free second allocation
    err = ppdb_base_memory_pool_free(pool, ptr2);
    assert(err == PPDB_OK);

    // Check final stats
    ppdb_base_memory_pool_get_stats(pool, &stats);
    assert(stats.total_freed == size1 + size2);
    assert(stats.current_usage == 0);
    assert(stats.peak_usage == size1 + size2);

    // Cleanup
    ppdb_base_memory_pool_destroy(pool);
}

// Test memory alignment
static void test_memory_alignment(void) {
    ppdb_error_t err;
    ppdb_base_memory_pool_t* pool = NULL;
    void* ptr = NULL;
    const size_t size = 1024;
    const size_t alignment = 64;

    // Create memory pool
    err = ppdb_base_memory_pool_create(&pool, 1024 * 1024);
    assert(err == PPDB_OK);

    // Allocate aligned memory
    err = ppdb_base_memory_pool_aligned_alloc(pool, size, alignment, &ptr);
    assert(err == PPDB_OK);
    assert(ptr != NULL);
    assert(((uintptr_t)ptr % alignment) == 0);

    // Free aligned memory
    err = ppdb_base_memory_pool_free(pool, ptr);
    assert(err == PPDB_OK);

    // Cleanup
    ppdb_base_memory_pool_destroy(pool);
}

// Test error handling
static void test_memory_errors(void) {
    ppdb_error_t err;
    ppdb_base_memory_pool_t* pool = NULL;
    void* ptr = NULL;

    // Test invalid parameters
    err = ppdb_base_memory_pool_create(NULL, 1024);
    assert(err == PPDB_BASE_ERR_PARAM);

    err = ppdb_base_memory_pool_create(&pool, 0);
    assert(err == PPDB_BASE_ERR_PARAM);

    // Create valid pool
    err = ppdb_base_memory_pool_create(&pool, 1024);
    assert(err == PPDB_OK);

    // Test invalid allocation
    err = ppdb_base_memory_pool_alloc(NULL, 1024, &ptr);
    assert(err == PPDB_BASE_ERR_PARAM);

    err = ppdb_base_memory_pool_alloc(pool, 0, &ptr);
    assert(err == PPDB_BASE_ERR_PARAM);

    err = ppdb_base_memory_pool_alloc(pool, 1024, NULL);
    assert(err == PPDB_BASE_ERR_PARAM);

    // Test allocation larger than pool
    err = ppdb_base_memory_pool_alloc(pool, 2048, &ptr);
    assert(err == PPDB_BASE_ERR_MEMORY);

    // Test invalid free
    err = ppdb_base_memory_pool_free(NULL, ptr);
    assert(err == PPDB_BASE_ERR_PARAM);

    err = ppdb_base_memory_pool_free(pool, NULL);
    assert(err == PPDB_BASE_ERR_PARAM);

    // Cleanup
    ppdb_base_memory_pool_destroy(pool);
}

int main(void) {
    printf("Testing memory pool basic operations...\n");
    test_memory_pool_basic();
    printf("PASSED\n");

    printf("Testing memory operations...\n");
    test_memory_operations();
    printf("PASSED\n");

    printf("Testing memory alignment...\n");
    test_memory_alignment();
    printf("PASSED\n");

    printf("Testing memory error handling...\n");
    test_memory_errors();
    printf("PASSED\n");

    return 0;
}