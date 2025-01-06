#include <cosmopolitan.h>
#include "internal/base.h"
#include "internal/storage.h"
#include "test_common.h"

// Test configuration
#define OPS_PER_THREAD 100
#define NUM_THREADS 4
#define TABLE_SIZE (1024 * 1024)

// Thread context
typedef struct {
    ppdb_storage_table_t* table;
    int thread_id;
    bool success;
} thread_ctx_t;

// Forward declarations
static void concurrent_worker(void* arg);

// Thread worker function
static void concurrent_worker(void* arg) {
    thread_ctx_t* ctx = (thread_ctx_t*)arg;
    
    for (int j = 0; j < OPS_PER_THREAD; j++) {
        char key[32], value[32];
        snprintf(key, sizeof(key), "key_%d_%d", ctx->thread_id, j);
        snprintf(value, sizeof(value), "value_%d_%d", ctx->thread_id, j);

        // Put operation
        if (ppdb_storage_put(ctx->table, key, strlen(key), value, strlen(value)) != PPDB_OK) {
            ctx->success = false;
            return;
        }

        // Get operation
        char result[32];
        size_t size = sizeof(result);
        if (ppdb_storage_get(ctx->table, key, strlen(key), result, &size) != PPDB_OK) {
            ctx->success = false;
            return;
        }

        // Verify
        if (memcmp(result, value, strlen(value)) != 0) {
            ctx->success = false;
            return;
        }
    }
}

// Test basic operations
static void test_memtable_basic(void) {
    ppdb_base_t* base = NULL;
    ppdb_storage_t* storage = NULL;
    ppdb_storage_table_t* table = NULL;

    // Initialize base layer
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    ASSERT_EQ(ppdb_base_init(&base, &base_config), PPDB_OK);

    // Initialize storage layer
    ppdb_storage_config_t storage_config = {
        .memtable_size = TABLE_SIZE,
        .block_size = PPDB_DEFAULT_BLOCK_SIZE,
        .cache_size = PPDB_DEFAULT_CACHE_SIZE,
        .write_buffer_size = PPDB_DEFAULT_WRITE_BUFFER_SIZE,
        .data_dir = PPDB_DEFAULT_DATA_DIR,
        .use_compression = PPDB_DEFAULT_USE_COMPRESSION,
        .sync_writes = PPDB_DEFAULT_SYNC_WRITES
    };
    ASSERT_EQ(ppdb_storage_init(&storage, base, &storage_config), PPDB_OK);

    // Create table
    ASSERT_EQ(ppdb_storage_table_create(storage, "test_table", &table), PPDB_OK);

    // Basic operations
    const char* key = "test_key";
    const char* value = "test_value";
    ASSERT_EQ(ppdb_storage_put(table, key, strlen(key), value, strlen(value)), PPDB_OK);

    char buffer[256];
    size_t size = sizeof(buffer);
    ASSERT_EQ(ppdb_storage_get(table, key, strlen(key), buffer, &size), PPDB_OK);
    ASSERT_EQ(memcmp(buffer, value, strlen(value)), 0);

    // Cleanup
    ppdb_storage_table_destroy(table);
    ppdb_storage_destroy(storage);
    ppdb_base_destroy(base);
}

// Test concurrent operations
static void test_memtable_concurrent(void) {
    ppdb_base_t* base = NULL;
    ppdb_storage_t* storage = NULL;
    ppdb_storage_table_t* table = NULL;
    ppdb_base_thread_t* threads[NUM_THREADS];
    thread_ctx_t thread_ctx[NUM_THREADS];

    // Initialize base layer
    ppdb_base_config_t base_config = {
        .memory_limit = 1024 * 1024,  // 1MB
        .thread_pool_size = 4,
        .thread_safe = true
    };
    ASSERT_EQ(ppdb_base_init(&base, &base_config), PPDB_OK);

    // Initialize storage layer
    ppdb_storage_config_t storage_config = {
        .memtable_size = TABLE_SIZE,
        .block_size = PPDB_DEFAULT_BLOCK_SIZE,
        .cache_size = PPDB_DEFAULT_CACHE_SIZE,
        .write_buffer_size = PPDB_DEFAULT_WRITE_BUFFER_SIZE,
        .data_dir = PPDB_DEFAULT_DATA_DIR,
        .use_compression = PPDB_DEFAULT_USE_COMPRESSION,
        .sync_writes = PPDB_DEFAULT_SYNC_WRITES
    };
    ASSERT_EQ(ppdb_storage_init(&storage, base, &storage_config), PPDB_OK);

    // Create table
    ASSERT_EQ(ppdb_storage_table_create(storage, "test_table", &table), PPDB_OK);

    // Create threads
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ctx[i].table = table;
        thread_ctx[i].thread_id = i;
        thread_ctx[i].success = true;
        ASSERT_EQ(ppdb_base_thread_create(&threads[i], concurrent_worker, &thread_ctx[i]), PPDB_OK);
    }

    // Wait for threads
    for (int i = 0; i < NUM_THREADS; i++) {
        ASSERT_EQ(ppdb_base_thread_join(threads[i], NULL), PPDB_OK);
        ASSERT_TRUE(thread_ctx[i].success);
    }

    // Verify results
    for (int i = 0; i < NUM_THREADS; i++) {
        for (int j = 0; j < OPS_PER_THREAD; j++) {
            char key[32], value[32], result[32];
            size_t size = sizeof(result);
            snprintf(key, sizeof(key), "key_%d_%d", i, j);
            snprintf(value, sizeof(value), "value_%d_%d", i, j);
            ASSERT_EQ(ppdb_storage_get(table, key, strlen(key), result, &size), PPDB_OK);
            ASSERT_EQ(memcmp(result, value, strlen(value)), 0);
        }
    }

    // Cleanup
    ppdb_storage_table_destroy(table);
    ppdb_storage_destroy(storage);
    ppdb_base_destroy(base);
}

int main(void) {
    printf("Running memtable tests...\n");
    test_memtable_basic();
    test_memtable_concurrent();
    printf("All memtable tests passed!\n");
    return 0;
} 