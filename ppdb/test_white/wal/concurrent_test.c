#include "ppdb/memtable.h"
#include "ppdb/wal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_THREADS 4
#define OPS_PER_THREAD 1000

typedef struct {
    ppdb_wal_t* wal;
    int thread_id;
} thread_arg_t;

// 线程函数：并发写入
static void* write_thread(void* arg) {
    thread_arg_t* targ = (thread_arg_t*)arg;
    char key[32], value[32];

    for (int i = 0; i < OPS_PER_THREAD; i++) {
        snprintf(key, sizeof(key), "key_%d_%d", targ->thread_id, i);
        snprintf(value, sizeof(value), "value_%d_%d", targ->thread_id, i);
        
        ppdb_error_t err = ppdb_wal_write(targ->wal, PPDB_WAL_PUT,
            (uint8_t*)key, strlen(key),
            (uint8_t*)value, strlen(value));
        assert(err == PPDB_OK);
    }

    return NULL;
}

// 测试并发写入
static void test_concurrent_write() {
    printf("Running test_concurrent_write...\n");

    // 创建 WAL
    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 4096,
        .sync_write = true
    };
    ppdb_wal_t* wal = NULL;
    assert(ppdb_wal_create(&config, &wal) == PPDB_OK);

    // 创建线程
    pthread_t threads[NUM_THREADS];
    thread_arg_t args[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].wal = wal;
        args[i].thread_id = i;
        pthread_create(&threads[i], NULL, write_thread, &args[i]);
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // 验证数据
    ppdb_memtable_t* table = NULL;
    assert(ppdb_memtable_create(1024 * 1024, &table) == PPDB_OK);
    assert(ppdb_wal_recover(wal, table) == PPDB_OK);

    // 检查所有数据
    char key[32], value[32];
    uint8_t buf[256];
    for (int t = 0; t < NUM_THREADS; t++) {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            snprintf(key, sizeof(key), "key_%d_%d", t, i);
            snprintf(value, sizeof(value), "value_%d_%d", t, i);
            
            size_t len = sizeof(buf);
            assert(ppdb_memtable_get(table, (uint8_t*)key, strlen(key), buf, &len) == PPDB_OK);
            assert(len == strlen(value));
            assert(memcmp(buf, value, len) == 0);
        }
    }

    ppdb_memtable_destroy(table);
    ppdb_wal_destroy(wal);
    printf("test_concurrent_write passed\n");
}

// 测试并发写入和归档
static void test_concurrent_write_archive() {
    printf("Running test_concurrent_write_archive...\n");

    // 创建 WAL
    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 1024,  // 小段大小，触发更频繁的切换
        .sync_write = true
    };
    ppdb_wal_t* wal = NULL;
    assert(ppdb_wal_create(&config, &wal) == PPDB_OK);

    // 创建写入线程
    pthread_t threads[NUM_THREADS];
    thread_arg_t args[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].wal = wal;
        args[i].thread_id = i;
        pthread_create(&threads[i], NULL, write_thread, &args[i]);
    }

    // 同时执行归档操作
    for (int i = 0; i < 5; i++) {
        usleep(100000);  // 100ms
        assert(ppdb_wal_archive(wal) == PPDB_OK);
    }

    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // 最后一次归档
    assert(ppdb_wal_archive(wal) == PPDB_OK);

    // 验证数据完整性
    ppdb_memtable_t* table = NULL;
    assert(ppdb_memtable_create(1024 * 1024, &table) == PPDB_OK);
    assert(ppdb_wal_recover(wal, table) == PPDB_OK);

    char key[32], value[32];
    uint8_t buf[256];
    for (int t = 0; t < NUM_THREADS; t++) {
        for (int i = 0; i < OPS_PER_THREAD; i++) {
            snprintf(key, sizeof(key), "key_%d_%d", t, i);
            snprintf(value, sizeof(value), "value_%d_%d", t, i);
            
            size_t len = sizeof(buf);
            assert(ppdb_memtable_get(table, (uint8_t*)key, strlen(key), buf, &len) == PPDB_OK);
            assert(len == strlen(value));
            assert(memcmp(buf, value, len) == 0);
        }
    }

    ppdb_memtable_destroy(table);
    ppdb_wal_destroy(wal);
    printf("test_concurrent_write_archive passed\n");
}

int main() {
    // 创建测试目录
    mkdir("test_wal", 0755);

    // 运行测试
    test_concurrent_write();
    test_concurrent_write_archive();

    // 清理测试目录
    system("rm -rf test_wal");

    printf("All concurrent WAL tests passed!\n");
    return 0;
} 