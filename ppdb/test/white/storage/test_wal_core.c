#include <cosmopolitan.h>
#include "ppdb/ppdb_kvstore.h"
#include "ppdb/ppdb_error.h"
#include "kvstore/internal/kvstore_wal.h"
#include "kvstore/internal/kvstore_wal_types.h"
#include "test/test_utils.h"

// 测试 WAL 文件格式
void test_wal_format(void) {
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = true
    };

    // 创建 WAL
    ASSERT_OK(ppdb_wal_create(&config, &wal));

    // 写入一些数据
    const char* key = "test_key";
    const char* value = "test_value";
    ASSERT_OK(ppdb_wal_write(wal, key, strlen(key), value, strlen(value)));

    // 验证文件格式
    wal_segment_t* segment = wal->segments;
    ASSERT_NOT_NULL(segment);
    
    // 读取段头部
    wal_segment_header_t header;
    ssize_t read_size = pread(segment->fd, &header, sizeof(header), 0);
    ASSERT_EQ(read_size, sizeof(header));
    
    // 验证魔数和版本
    ASSERT_EQ(header.magic, WAL_MAGIC);
    ASSERT_EQ(header.version, WAL_VERSION);

    // 清理
    ppdb_wal_destroy(wal);
    rmdir("test_wal");
}

// 测试段文件操作
void test_segment_ops(void) {
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = true
    };

    // 创建 WAL
    ASSERT_OK(ppdb_wal_create(&config, &wal));

    // 写入足够多的数据以创建新段
    const char* key = "test_key";
    const char* value = "test_value";
    for (int i = 0; i < 100; i++) {
        ASSERT_OK(ppdb_wal_write(wal, key, strlen(key), value, strlen(value)));
    }

    // 验证段数量
    ASSERT_EQ(wal->segment_count, 2);

    // 验证段文件存在
    wal_segment_t* curr = wal->segments;
    while (curr) {
        struct stat st;
        ASSERT_EQ(stat(curr->filename, &st), 0);
        curr = curr->next;
    }

    // 清理
    ppdb_wal_destroy(wal);
    rmdir("test_wal");
}

// 测试基本读写
void test_basic_rw(void) {
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = true
    };

    // 创建 WAL
    ASSERT_OK(ppdb_wal_create(&config, &wal));

    // 写入数据
    const char* key = "test_key";
    const char* value = "test_value";
    ASSERT_OK(ppdb_wal_write(wal, key, strlen(key), value, strlen(value)));

    // 读取数据
    void* read_key = NULL;
    void* read_value = NULL;
    size_t key_size = 0;
    size_t value_size = 0;
    
    wal_segment_t* segment = wal->segments;
    ASSERT_NOT_NULL(segment);
    
    // 跳过段头部
    size_t offset = sizeof(wal_segment_header_t);
    
    // 读取记录头部
    wal_record_header_t header;
    ssize_t read_size = pread(segment->fd, &header, sizeof(header), offset);
    ASSERT_EQ(read_size, sizeof(header));
    
    // 读取键值
    read_key = malloc(header.key_size);
    read_value = malloc(header.value_size);
    ASSERT_NOT_NULL(read_key);
    ASSERT_NOT_NULL(read_value);
    
    read_size = pread(segment->fd, read_key, header.key_size, 
                     offset + sizeof(header));
    ASSERT_EQ(read_size, header.key_size);
    
    read_size = pread(segment->fd, read_value, header.value_size,
                     offset + sizeof(header) + header.key_size);
    ASSERT_EQ(read_size, header.value_size);
    
    // 验证数据
    ASSERT_EQ(memcmp(read_key, key, header.key_size), 0);
    ASSERT_EQ(memcmp(read_value, value, header.value_size), 0);

    // 清理
    free(read_key);
    free(read_value);
    ppdb_wal_destroy(wal);
    rmdir("test_wal");
}

// 测试校验和机制
void test_checksum(void) {
    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = "test_wal",
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = true
    };

    // 创建 WAL
    ASSERT_OK(ppdb_wal_create(&config, &wal));

    // 写入数据
    const char* key = "test_key";
    const char* value = "test_value";
    ASSERT_OK(ppdb_wal_write(wal, key, strlen(key), value, strlen(value)));

    // 读取并验证校验和
    wal_segment_t* segment = wal->segments;
    ASSERT_NOT_NULL(segment);
    
    // 跳过段头部
    size_t offset = sizeof(wal_segment_header_t);
    
    // 读取记录头部
    wal_record_header_t header;
    ssize_t read_size = pread(segment->fd, &header, sizeof(header), offset);
    ASSERT_EQ(read_size, sizeof(header));
    
    // 保存原始校验和
    uint32_t saved_checksum = header.checksum;
    
    // 计算校验和
    void* read_key = malloc(header.key_size);
    void* read_value = malloc(header.value_size);
    ASSERT_NOT_NULL(read_key);
    ASSERT_NOT_NULL(read_value);
    
    read_size = pread(segment->fd, read_key, header.key_size,
                     offset + sizeof(header));
    ASSERT_EQ(read_size, header.key_size);
    
    read_size = pread(segment->fd, read_value, header.value_size,
                     offset + sizeof(header) + header.key_size);
    ASSERT_EQ(read_size, header.value_size);
    
    header.checksum = 0;
    uint32_t computed_checksum = calculate_crc32(&header, sizeof(header));
    computed_checksum = calculate_crc32(read_key, header.key_size);
    computed_checksum = calculate_crc32(read_value, header.value_size);
    
    // 验证校验和
    ASSERT_EQ(computed_checksum, saved_checksum);

    // 清理
    free(read_key);
    free(read_value);
    ppdb_wal_destroy(wal);
    rmdir("test_wal");
}

int main(void) {
    TEST_INIT();
    
    RUN_TEST(test_wal_format);
    RUN_TEST(test_segment_ops);
    RUN_TEST(test_basic_rw);
    RUN_TEST(test_checksum);
    
    TEST_SUMMARY();
    return 0;
} 