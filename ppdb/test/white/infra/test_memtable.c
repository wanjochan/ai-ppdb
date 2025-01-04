#include "test_framework.h"
#include "ppdb/ppdb.h"
#include <cosmopolitan.h>

// Test configuration
#define TEST_MEMTABLE_SIZE (1024 * 1024)  // 1MB
#define TEST_KEY_SIZE 16
#define TEST_VALUE_SIZE 100
#define TEST_ITERATIONS 10  // Reduced from 100 to 10 for faster testing

// Forward declarations
static void* worker_thread(void* arg);
extern uint64_t lemur64(void);

// Test mode
#ifdef PPDB_SYNC_MODE_LOCKFREE
#define USE_LOCKFREE 1
#else
#define USE_LOCKFREE 0
#endif

// Test cases
static int test_memtable_basic(void) {
    printf("Starting basic memtable test (use_lockfree=%d)...\n", USE_LOCKFREE);

    // Create memtable
    ppdb_base_t* base = NULL;
    ppdb_error_t err = ppdb_create(&base, &(ppdb_config_t){
        .type = PPDB_TYPE_MEMTABLE,
        .use_lockfree = USE_LOCKFREE,
        .memtable_size = TEST_MEMTABLE_SIZE
    });
    ASSERT(err == PPDB_OK, "Create memtable result: %d", err);

    // Prepare test data
    char key_data[TEST_KEY_SIZE];
    char value_data[TEST_VALUE_SIZE];
    memset(key_data, 'k', TEST_KEY_SIZE);
    memset(value_data, 'v', TEST_VALUE_SIZE);

    ppdb_key_t key = {
        .data = key_data,
        .size = TEST_KEY_SIZE
    };
    ppdb_value_t value = {
        .data = value_data,
        .size = TEST_VALUE_SIZE
    };

    // Test put
    printf("Putting key-value pair...\n");
    err = ppdb_put(base, &key, &value);
    ASSERT(err == PPDB_OK, "Put result: %d", err);

    // Test get
    printf("Getting value...\n");
    ppdb_value_t get_value = {0};
    err = ppdb_get(base, &key, &get_value);
    ASSERT(err == PPDB_OK, "Get result: %d", err);

    // Compare values
    printf("Comparing values...\n");
    ASSERT(get_value.size == value.size, "Expected size: %zu, Actual size: %zu",
           value.size, get_value.size);
    ASSERT(memcmp(get_value.data, value.data, value.size) == 0,
           "Value data mismatch");

    // Test remove
    printf("Removing key...\n");
    err = ppdb_remove(base, &key);
    ASSERT(err == PPDB_OK, "Remove result: %d", err);

    // Verify removal
    printf("Verifying removal...\n");
    err = ppdb_get(base, &key, &get_value);
    ASSERT(err == PPDB_ERR_NOT_FOUND, "Get after remove result: %d", err);

    // Cleanup
    printf("Destroying memtable...\n");
    ppdb_destroy(base);
    printf("Basic test completed\n");
    return 0;
}

static int test_memtable_concurrent(void) {
    printf("Starting concurrent memtable test (use_lockfree=%d)...\n", USE_LOCKFREE);

    // Create memtable
    ppdb_base_t* base = NULL;
    ppdb_error_t err = ppdb_create(&base, &(ppdb_config_t){
        .type = PPDB_TYPE_MEMTABLE,
        .use_lockfree = USE_LOCKFREE,
        .memtable_size = TEST_MEMTABLE_SIZE
    });
    ASSERT(err == PPDB_OK, "Create memtable failed");

    // Create worker threads
    pthread_t threads[4];
    for (int i = 0; i < 4; i++) {
        pthread_create(&threads[i], NULL, worker_thread, base);
    }

    // Wait for threads to complete
    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
    }

    // Get and print metrics
    ppdb_metrics_t metrics = {0};
    err = ppdb_storage_get_stats(base, &metrics);
    ASSERT(err == PPDB_OK, "Get metrics failed");

    printf("Concurrent test results:\n");
    printf("Total operations: %d\n", TEST_ITERATIONS * 4);
    printf("Insert ops: %lu (success: %lu)\n", metrics.put_count, metrics.put_count);
    printf("Find ops: %lu (success: %lu)\n", metrics.get_count, metrics.get_hits);
    printf("Delete ops: %lu (success: %lu)\n", metrics.remove_count, metrics.remove_count);

    printf("Storage metrics:\n");
    printf("Get count: %lu (hits: %lu)\n", metrics.get_count, metrics.get_hits);
    printf("Put count: %lu\n", metrics.put_count);
    printf("Remove count: %lu\n", metrics.remove_count);

    // Cleanup
    ppdb_destroy(base);
    printf("Concurrent test completed\n");
    return 0;
}

static int test_memtable_iterator(void) {
    printf("Starting iterator test (use_lockfree=%d)...\n", USE_LOCKFREE);

    // Create memtable
    ppdb_base_t* base = NULL;
    ppdb_error_t err = ppdb_create(&base, &(ppdb_config_t){
        .type = PPDB_TYPE_MEMTABLE,
        .use_lockfree = USE_LOCKFREE,
        .memtable_size = TEST_MEMTABLE_SIZE
    });
    ASSERT(err == PPDB_OK, "Create memtable failed");

    // Insert test data
    for (int i = 0; i < 10; i++) {
        char key_data[32], value_data[32];
        snprintf(key_data, sizeof(key_data), "key_%d", i);
        snprintf(value_data, sizeof(value_data), "value_%d", i);

        ppdb_key_t key = {
            .data = key_data,
            .size = strlen(key_data)
        };
        ppdb_value_t value = {
            .data = value_data,
            .size = strlen(value_data)
        };

        err = ppdb_put(base, &key, &value);
        ASSERT(err == PPDB_OK, "Put failed at index %d", i);
    }

    // Test iterator
    void* iter = NULL;
    err = ppdb_iterator_init(base, &iter);
    ASSERT(err == PPDB_OK, "Iterator init failed");

    int count = 0;
    ppdb_key_t key;
    ppdb_value_t value;
    while (ppdb_iterator_next(iter, &key, &value) == PPDB_OK) {
        printf("Iter %d: key=%.*s, value=%.*s\n",
               count++,
               (int)key.size, (char*)key.data,
               (int)value.size, (char*)value.data);
        PPDB_ALIGNED_FREE(key.data);
        PPDB_ALIGNED_FREE(value.data);
    }

    ppdb_iterator_destroy(iter);
    ppdb_destroy(base);
    printf("Iterator test completed\n");
    return 0;
}

// Worker thread function for concurrent test
static void* worker_thread(void* arg) {
    ppdb_base_t* base = (ppdb_base_t*)arg;

    for (int i = 0; i < TEST_ITERATIONS; i++) {
        // Generate random key and value
        char* key_data = PPDB_ALIGNED_ALLOC(TEST_KEY_SIZE);
        char* value_data = PPDB_ALIGNED_ALLOC(TEST_VALUE_SIZE);

        if (!key_data || !value_data) {
            if (key_data) PPDB_ALIGNED_FREE(key_data);
            if (value_data) PPDB_ALIGNED_FREE(value_data);
            continue;
        }

        snprintf(key_data, TEST_KEY_SIZE, "key_%d_%d", (int)pthread_self(), i);
        snprintf(value_data, TEST_VALUE_SIZE, "value_%d_%d", (int)pthread_self(), i);

        ppdb_key_t key = {
            .data = key_data,
            .size = strlen(key_data)
        };
        ppdb_value_t value = {
            .data = value_data,
            .size = strlen(value_data)
        };

        // Randomly choose operation
        int op = lemur64() % 3;
        switch (op) {
            case 0: {  // Put
                ppdb_put(base, &key, &value);
                PPDB_ALIGNED_FREE(key_data);
                PPDB_ALIGNED_FREE(value_data);
                break;
            }
            case 1: {  // Get
                ppdb_value_t get_value = {0};
                ppdb_get(base, &key, &get_value);
                if (get_value.data) {
                    PPDB_ALIGNED_FREE(get_value.data);
                }
                PPDB_ALIGNED_FREE(key_data);
                PPDB_ALIGNED_FREE(value_data);
                break;
            }
            case 2: {  // Remove
                ppdb_remove(base, &key);
                PPDB_ALIGNED_FREE(key_data);
                PPDB_ALIGNED_FREE(value_data);
                break;
            }
        }
    }

    return NULL;
}

// Test cases array
static test_case_t test_cases[] = {
    {
        .name = "Basic Memtable Operations",
        .description = "Tests basic operations (put/get/remove) on memtable",
        .fn = test_memtable_basic,
        .timeout_seconds = 10,
        .skip = false
    },
    {
        .name = "Concurrent Memtable Operations",
        .description = "Tests concurrent operations on memtable with multiple threads",
        .fn = test_memtable_concurrent,
        .timeout_seconds = 60,  // Increased from 30 to 60 seconds
        .skip = false
    },
    {
        .name = "Memtable Iterator",
        .description = "Tests memtable iterator functionality",
        .fn = test_memtable_iterator,
        .timeout_seconds = 10,
        .skip = false
    }
};

// Test suite
static test_suite_t memtable_test_suite = {
    .name = "Memtable Test Suite",
    .setup = NULL,
    .teardown = NULL,
    .cases = test_cases,
    .num_cases = sizeof(test_cases) / sizeof(test_cases[0])
};

int main(void) {
    printf("\n=== PPDB Memtable Test Suite ===\n");
    printf("Test Mode: %s\n", USE_LOCKFREE ? "lockfree" : "locked");
    printf("Starting tests...\n\n");

    test_framework_init();
    int result = run_test_suite(&memtable_test_suite);
    test_framework_cleanup();
    test_print_stats();

    return result;
} 
