#include <cosmopolitan.h>
#include "test_framework.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_kvstore.h"
#include "kvstore/internal/kvstore_wal.h"
#include "kvstore/internal/kvstore_memtable.h"

// 测试基本写入和恢复
static void test_basic_write_recover() {
    ppdb_log_info("Running test_basic_write_recover...");

    // 创建 WAL
    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 4096,
        .sync_write = true
    };
    ppdb_wal_t* wal = NULL;
    assert(ppdb_wal_create(&config, &wal) == PPDB_OK);

    // 写入一些数据
    const char* key1 = "key1";
    const char* value1 = "value1";
    assert(ppdb_wal_write(wal, PPDB_WAL_PUT, 
        (uint8_t*)key1, strlen(key1),
        (uint8_t*)value1, strlen(value1)) == PPDB_OK);

    const char* key2 = "key2";
    const char* value2 = "value2";
    assert(ppdb_wal_write(wal, PPDB_WAL_PUT,
        (uint8_t*)key2, strlen(key2),
        (uint8_t*)value2, strlen(value2)) == PPDB_OK);

    // 创建 MemTable 并恢复数据
    ppdb_memtable_t* table = NULL;
    assert(ppdb_memtable_create(4096, &table) == PPDB_OK);
    assert(ppdb_wal_recover(wal, table) == PPDB_OK);

    // 验证恢复的数据
    uint8_t buf[256];
    size_t len = sizeof(buf);
    assert(ppdb_memtable_get(table, (uint8_t*)key1, strlen(key1), buf, &len) == PPDB_OK);
    assert(len == strlen(value1));
    assert(memcmp(buf, value1, len) == 0);

    len = sizeof(buf);
    assert(ppdb_memtable_get(table, (uint8_t*)key2, strlen(key2), buf, &len) == PPDB_OK);
    assert(len == strlen(value2));
    assert(memcmp(buf, value2, len) == 0);

    ppdb_memtable_destroy(table);
    ppdb_wal_destroy(wal);
    ppdb_log_info("test_basic_write_recover passed");
}

// 测试文件分段
static void test_segment_switch() {
    ppdb_log_info("Running test_segment_switch...");

    // 创建 WAL，使用小的段大小
    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 64,  // 非常小的段大小，强制切换
        .sync_write = true
    };
    ppdb_wal_t* wal = NULL;
    assert(ppdb_wal_create(&config, &wal) == PPDB_OK);

    // 写入足够多的数据触发分段
    char key[32], value[32];
    for (int i = 0; i < 10; i++) {
        strlcpy(key, "key", sizeof(key));
        strlcat(key, tostring(i), sizeof(key));
        strlcpy(value, "value", sizeof(value));
        strlcat(value, tostring(i), sizeof(value));
        assert(ppdb_wal_write(wal, PPDB_WAL_PUT,
            (uint8_t*)key, strlen(key),
            (uint8_t*)value, strlen(value)) == PPDB_OK);
    }

    // 创建 MemTable 并恢复数据
    ppdb_memtable_t* table = NULL;
    assert(ppdb_memtable_create(4096, &table) == PPDB_OK);
    assert(ppdb_wal_recover(wal, table) == PPDB_OK);

    // 验证所有数据都正确恢复
    uint8_t buf[256];
    for (int i = 0; i < 10; i++) {
        strlcpy(key, "key", sizeof(key));
        strlcat(key, tostring(i), sizeof(key));
        strlcpy(value, "value", sizeof(value));
        strlcat(value, tostring(i), sizeof(value));
        size_t len = sizeof(buf);
        assert(ppdb_memtable_get(table, (uint8_t*)key, strlen(key), buf, &len) == PPDB_OK);
        assert(len == strlen(value));
        assert(memcmp(buf, value, len) == 0);
    }

    ppdb_memtable_destroy(table);
    ppdb_wal_destroy(wal);
    ppdb_log_info("test_segment_switch passed");
}

// 测试崩溃恢复
static void test_crash_recovery() {
    printf("Running test_crash_recovery...\n");

    // 第一阶段：写入数据
    {
        ppdb_wal_config_t config = {
            .dir_path = "test_wal",
            .segment_size = 4096,
            .sync_write = true
        };
        ppdb_wal_t* wal = NULL;
        assert(ppdb_wal_create(&config, &wal) == PPDB_OK);

        const char* key = "crash_key";
        const char* value = "crash_value";
        assert(ppdb_wal_write(wal, PPDB_WAL_PUT,
            (uint8_t*)key, strlen(key),
            (uint8_t*)value, strlen(value)) == PPDB_OK);

        ppdb_wal_destroy(wal);
    }

    // 第二阶段：模拟崩溃后恢复
    {
        ppdb_wal_config_t config = {
            .dir_path = "test_wal",
            .segment_size = 4096,
            .sync_write = true
        };
        ppdb_wal_t* wal = NULL;
        assert(ppdb_wal_create(&config, &wal) == PPDB_OK);

        ppdb_memtable_t* table = NULL;
        assert(ppdb_memtable_create(4096, &table) == PPDB_OK);
        assert(ppdb_wal_recover(wal, table) == PPDB_OK);

        // 验证数据
        const char* key = "crash_key";
        const char* value = "crash_value";
        uint8_t buf[256];
        size_t len = sizeof(buf);
        assert(ppdb_memtable_get(table, (uint8_t*)key, strlen(key), buf, &len) == PPDB_OK);
        assert(len == strlen(value));
        assert(memcmp(buf, value, len) == 0);

        ppdb_memtable_destroy(table);
        ppdb_wal_destroy(wal);
    }

    printf("test_crash_recovery passed\n");
}

// 测试归档功能
static void test_archive() {
    printf("Running test_archive...\n");

    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 64,  // 小的段大小，快速产生多个文件
        .sync_write = true
    };
    ppdb_wal_t* wal = NULL;
    assert(ppdb_wal_create(&config, &wal) == PPDB_OK);

    // 写入数据产生多个段文件
    char key[32], value[32];
    for (int i = 0; i < 20; i++) {
        snprintf(key, sizeof(key), "archive_key%d", i);
        snprintf(value, sizeof(value), "archive_value%d", i);
        assert(ppdb_wal_write(wal, PPDB_WAL_PUT,
            (uint8_t*)key, strlen(key),
            (uint8_t*)value, strlen(value)) == PPDB_OK);
    }

    // 执行归档
    assert(ppdb_wal_archive(wal) == PPDB_OK);

    // 验证归档后的恢复
    ppdb_memtable_t* table = NULL;
    assert(ppdb_memtable_create(4096, &table) == PPDB_OK);
    assert(ppdb_wal_recover(wal, table) == PPDB_OK);

    // 验证数据完整性
    uint8_t buf[256];
    for (int i = 0; i < 20; i++) {
        snprintf(key, sizeof(key), "archive_key%d", i);
        snprintf(value, sizeof(value), "archive_value%d", i);
        size_t len = sizeof(buf);
        assert(ppdb_memtable_get(table, (uint8_t*)key, strlen(key), buf, &len) == PPDB_OK);
        assert(len == strlen(value));
        assert(memcmp(buf, value, len) == 0);
    }

    ppdb_memtable_destroy(table);
    ppdb_wal_destroy(wal);
    printf("test_archive passed\n");
}

int main() {
    // 创建测试目录
    mkdir("test_wal", 0755);

    // 运行测试
    test_basic_write_recover();
    test_segment_switch();
    test_crash_recovery();
    test_archive();

    // 清理测试目录
    system("rm -rf test_wal");

    printf("All WAL tests passed!\n");
    return 0;
} 