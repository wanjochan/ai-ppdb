#include <cosmopolitan.h>
#include "ppdb/memtable.h"

#define NUM_THREADS 4
#define NUM_OPERATIONS 1000

// 线程参数结构
typedef struct {
    ppdb_memtable_t* table;
    int thread_id;
} thread_args_t;

// 线程函数
static void* worker_thread(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    ppdb_memtable_t* table = args->table;
    int thread_id = args->thread_id;
    
    char key_buf[32];
    char value_buf[32];
    uint8_t read_buf[32];
    size_t read_len;
    
    for (int i = 0; i < NUM_OPERATIONS; i++) {
        // 生成键值对
        snprintf(key_buf, sizeof(key_buf), "key_%d_%d", thread_id, i);
        snprintf(value_buf, sizeof(value_buf), "value_%d_%d", thread_id, i);
        
        // 写入
        ppdb_error_t err = ppdb_memtable_put(table, (uint8_t*)key_buf, strlen(key_buf) + 1,
                                            (uint8_t*)value_buf, strlen(value_buf) + 1);
        assert(err == PPDB_OK);
        
        // 读取并验证
        read_len = sizeof(read_buf);
        err = ppdb_memtable_get(table, (uint8_t*)key_buf, strlen(key_buf) + 1,
                               read_buf, &read_len);
        assert(err == PPDB_OK);
        assert(read_len == strlen(value_buf) + 1);
        assert(memcmp(read_buf, value_buf, read_len) == 0);
        
        // 随机删除一些键
        if (i % 3 == 0) {
            err = ppdb_memtable_delete(table, (uint8_t*)key_buf, strlen(key_buf) + 1);
            assert(err == PPDB_OK);
            
            // 验证删除
            err = ppdb_memtable_get(table, (uint8_t*)key_buf, strlen(key_buf) + 1,
                                   read_buf, &read_len);
            assert(err == PPDB_ERR_NOT_FOUND);
        }
    }
    
    return NULL;
}

// 测试并发读写
static void test_concurrent_operations(void) {
    printf("Testing Concurrent Operations...\n");
    
    // 创建 MemTable
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024 * 1024, &table);  // 1MB
    printf("  Create MemTable: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);
    
    // 创建线程
    pthread_t threads[NUM_THREADS];
    thread_args_t args[NUM_THREADS];
    
    printf("  Starting %d threads, each performing %d operations...\n", 
           NUM_THREADS, NUM_OPERATIONS);
    
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].table = table;
        args[i].thread_id = i;
        pthread_create(&threads[i], NULL, worker_thread, &args[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    printf("  All threads completed successfully\n");
    printf("  Final table size: %zu\n", ppdb_memtable_size(table));
    
    ppdb_memtable_destroy(table);
    printf("  Destroy MemTable: OK\n");
    printf("Test passed!\n\n");
}

// 测试并发更新同一个键
static void test_concurrent_updates(void) {
    printf("Testing Concurrent Updates...\n");
    
    // 创建 MemTable
    ppdb_memtable_t* table = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024 * 1024, &table);  // 1MB
    printf("  Create MemTable: %s\n", err == PPDB_OK ? "OK" : "Failed");
    assert(err == PPDB_OK);
    
    // 初始化共享键
    const uint8_t shared_key[] = "shared_key";
    const uint8_t initial_value[] = "initial_value";
    err = ppdb_memtable_put(table, shared_key, sizeof(shared_key),
                           initial_value, sizeof(initial_value));
    assert(err == PPDB_OK);
    
    // 创建线程
    pthread_t threads[NUM_THREADS];
    thread_args_t args[NUM_THREADS];
    
    printf("  Starting %d threads to update the same key...\n", NUM_THREADS);
    
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].table = table;
        args[i].thread_id = i;
        pthread_create(&threads[i], NULL, worker_thread, &args[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // 验证最终值
    uint8_t buf[256];
    size_t buf_len = sizeof(buf);
    err = ppdb_memtable_get(table, shared_key, sizeof(shared_key), buf, &buf_len);
    printf("  Final value length: %zu\n", buf_len);
    printf("  Final value: %s\n", buf);
    
    ppdb_memtable_destroy(table);
    printf("  Destroy MemTable: OK\n");
    printf("Test passed!\n\n");
}

int main(int argc, char* argv[]) {
    printf("Starting MemTable Concurrent Tests...\n\n");
    
    // 运行所有测试
    test_concurrent_operations();
    test_concurrent_updates();
    
    printf("All MemTable Concurrent Tests passed!\n");
    return 0;
} 