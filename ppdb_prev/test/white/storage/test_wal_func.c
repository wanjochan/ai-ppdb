#include <cosmopolitan.h>
#include "ppdb/ppdb.h"
#include "internal/wal.h"
#include "test/white/test_framework.h"

// 测试段管理
void test_segment_management(void) {
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = true
    };

    // 创建 WAL
    ASSERT_EQ(ppdb_wal_create(&config, &wal), PPDB_OK);

    // 写入数据直到创建多个段
    const char* key = "test_key";
    const char* value = "test_value";
    for (int i = 0; i < 200; i++) {
        ASSERT_EQ(ppdb_wal_write(wal, key, strlen(key), value, strlen(value)), PPDB_OK);
    }

    // 验证段数量不超过最大值
    ASSERT(wal->segment_count <= config.max_segments, "Segment count exceeds maximum");

    // 验证段大小不超过限制
    wal_segment_t* curr = wal->segments;
    while (curr) {
        ASSERT(curr->size <= config.segment_size, "Segment size exceeds limit");
        curr = curr->next;
    }

    // 清理
    ppdb_wal_destroy(wal);
    rmdir("test_wal");
}

// 测试写入缓冲区
void test_write_buffer(void) {
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = false  // 关闭同步写入以测试缓冲
    };

    // 创建 WAL
    ASSERT_EQ(ppdb_wal_create(&config, &wal), PPDB_OK);

    // 写入数据
    const char* key = "test_key";
    const char* value = "test_value";
    ASSERT_EQ(ppdb_wal_write(wal, key, strlen(key), value, strlen(value)), PPDB_OK);

    // 清理
    ppdb_wal_destroy(wal);
    rmdir("test_wal");
}

// 测试基本恢复
void test_basic_recovery(void) {
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = true
    };

    // 创建 WAL
    ASSERT_EQ(ppdb_wal_create(&config, &wal), PPDB_OK);

    // 写入数据
    const char* key = "test_key";
    const char* value = "test_value";
    ASSERT_EQ(ppdb_wal_write(wal, key, strlen(key), value, strlen(value)), PPDB_OK);

    // 恢复数据
    ASSERT_EQ(ppdb_wal_recover(wal, NULL, NULL), PPDB_OK);

    // 清理
    ppdb_wal_destroy(wal);
    rmdir("test_wal");
}

int main(void) {
    test_framework_init();

    test_segment_management();
    test_write_buffer();
    test_basic_recovery();

    test_framework_cleanup();
    return 0;
} 