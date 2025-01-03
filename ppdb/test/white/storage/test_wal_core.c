#include <cosmopolitan.h>
#include "ppdb/ppdb.h"
#include "ppdb/ppdb_sync.h"
#include "kvstore/internal/kvstore_wal.h"
#include "kvstore/internal/kvstore_wal_types.h"
#include "test/white/test_framework.h"
#include "test/test_utils.h"

// WAL 错误码定义
#define WAL_ERR_INVALID_CONFIG PPDB_ERR_INVALID_CONFIG
#define WAL_ERR_INVALID_ARGUMENT PPDB_ERR_INVALID_ARG

// 测试目录设置和清理
static void setup_test_dir(const char* dir_path) {
    char abs_path[PATH_MAX];
    if (_getcwd(abs_path, sizeof(abs_path)) == NULL) {
        return;
    }
    strlcat(abs_path, "\\", sizeof(abs_path));
    strlcat(abs_path, dir_path, sizeof(abs_path));

    // 删除已存在的目录及其内容
    test_remove_dir(abs_path);
    
    // 创建新的空目录
    _mkdir(abs_path);
}

static void cleanup_test_dir(const char* dir_path) {
    char abs_path[PATH_MAX];
    if (_getcwd(abs_path, sizeof(abs_path)) == NULL) {
        return;
    }
    strlcat(abs_path, "\\", sizeof(abs_path));
    strlcat(abs_path, dir_path, sizeof(abs_path));
    test_remove_dir(abs_path);
}

// 测试 WAL 文件格式
int test_wal_format(void) {
    const char* test_dir = "test_wal";
    char abs_path[PATH_MAX];
    if (_getcwd(abs_path, sizeof(abs_path)) == NULL) {
        return -1;
    }
    strlcat(abs_path, "\\", sizeof(abs_path));
    strlcat(abs_path, test_dir, sizeof(abs_path));

    setup_test_dir(test_dir);

    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = abs_path,
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = true
    };

    // 创建 WAL
    ASSERT_OK(ppdb_wal_create(&config, &wal));
    ASSERT_NOT_NULL(wal->segments);  // 确保创建了第一个段

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
    cleanup_test_dir(test_dir);
    return 0;
}

// 测试段文件操作
int test_segment_ops(void) {
    const char* test_dir = "test_wal";
    setup_test_dir(test_dir);

    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = test_dir,
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = true
    };

    // 创建 WAL
    ASSERT_OK(ppdb_wal_create(&config, &wal));
    ASSERT_NOT_NULL(wal->segments);

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
    cleanup_test_dir(test_dir);
    return 0;
}

// 测试基本读写
int test_basic_rw(void) {
    const char* test_dir = "test_wal";
    setup_test_dir(test_dir);

    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = test_dir,
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = true
    };

    // 创建 WAL
    ASSERT_OK(ppdb_wal_create(&config, &wal));
    ASSERT_NOT_NULL(wal->segments);

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
    cleanup_test_dir(test_dir);
    return 0;
}

// 测试校验和机制
int test_checksum(void) {
    const char* test_dir = "test_wal";
    setup_test_dir(test_dir);

    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = test_dir,
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = true
    };

    // 创建 WAL
    ASSERT_OK(ppdb_wal_create(&config, &wal));
    ASSERT_NOT_NULL(wal->segments);

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
    computed_checksum = calculate_crc32_update(computed_checksum, read_key, header.key_size);
    computed_checksum = calculate_crc32_update(computed_checksum, read_value, header.value_size);
    
    // 验证校验和
    ASSERT_EQ(computed_checksum, saved_checksum);

    // 清理
    free(read_key);
    free(read_value);
    ppdb_wal_destroy(wal);
    cleanup_test_dir(test_dir);
    return 0;
}

// 测试总大小跟踪
int test_total_size(void) {
    const char* test_dir = "test_wal";
    setup_test_dir(test_dir);

    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = test_dir,
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = true
    };

    // 创建 WAL
    ASSERT_OK(ppdb_wal_create(&config, &wal));
    ASSERT_NOT_NULL(wal->segments);
    ASSERT_EQ(wal->total_size, 0);

    // 写入数据并验证大小增长
    const char* key = "test_key";
    const char* value = "test_value";
    size_t record_size = sizeof(wal_record_header_t) + strlen(key) + strlen(value);
    
    ASSERT_OK(ppdb_wal_write(wal, key, strlen(key), value, strlen(value)));
    ASSERT_EQ(wal->total_size, record_size);

    // 写入更多数据
    ASSERT_OK(ppdb_wal_write(wal, key, strlen(key), value, strlen(value)));
    ASSERT_EQ(wal->total_size, record_size * 2);

    // 清理
    ppdb_wal_destroy(wal);
    cleanup_test_dir(test_dir);
    return 0;
}

// 测试段清理
int test_segment_cleanup(void) {
    const char* test_dir = "test_wal";
    setup_test_dir(test_dir);

    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = test_dir,
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = true
    };

    // 创建 WAL
    ASSERT_OK(ppdb_wal_create(&config, &wal));
    ASSERT_NOT_NULL(wal->segments);

    // 写入足够多的数据以创建多个段
    const char* key = "test_key";
    const char* value = "test_value";
    for (int i = 0; i < 200; i++) {
        ASSERT_OK(ppdb_wal_write(wal, key, strlen(key), value, strlen(value)));
    }

    // 验证段数量不超过最大限制
    ASSERT_EQ(wal->segment_count, config.max_segments);

    // 验证旧段被清理
    DIR* dir = opendir(config.dir_path);
    ASSERT_NOT_NULL(dir);
    
    int segment_files = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".wal") != NULL) {
            segment_files++;
        }
    }
    closedir(dir);
    
    ASSERT_EQ(segment_files, config.max_segments);

    // 清理
    ppdb_wal_destroy(wal);
    cleanup_test_dir(test_dir);
    return 0;
}

// 测试错误处理
int test_error_handling(void) {
    const char* test_dir = "test_wal";
    setup_test_dir(test_dir);

    ppdb_wal_t* wal = NULL;
    ppdb_wal_config_t config = {
        .dir_path = test_dir,
        .segment_size = 4096,
        .max_segments = 2,
        .sync_write = true
    };

    // 测试无效配置
    ppdb_wal_config_t invalid_config = {
        .dir_path = "",
        .segment_size = 0,
        .max_segments = 0,
        .sync_write = true
    };
    ASSERT_EQ(ppdb_wal_create(&invalid_config, &wal), WAL_ERR_INVALID_CONFIG);

    // 测试无效参数
    ASSERT_EQ(ppdb_wal_create(NULL, &wal), WAL_ERR_INVALID_ARGUMENT);
    ASSERT_EQ(ppdb_wal_create(&config, NULL), WAL_ERR_INVALID_ARGUMENT);

    // 创建有效的 WAL
    ASSERT_OK(ppdb_wal_create(&config, &wal));
    ASSERT_NOT_NULL(wal->segments);

    // 测试无效的写入参数
    ASSERT_EQ(ppdb_wal_write(NULL, "key", 3, "value", 5), WAL_ERR_INVALID_ARGUMENT);
    ASSERT_EQ(ppdb_wal_write(wal, NULL, 3, "value", 5), WAL_ERR_INVALID_ARGUMENT);
    ASSERT_EQ(ppdb_wal_write(wal, "key", 0, "value", 5), WAL_ERR_INVALID_ARGUMENT);
    ASSERT_EQ(ppdb_wal_write(wal, "key", 3, NULL, 5), WAL_ERR_INVALID_ARGUMENT);
    ASSERT_EQ(ppdb_wal_write(wal, "key", 3, "value", 0), WAL_ERR_INVALID_ARGUMENT);

    // 清理
    ppdb_wal_destroy(wal);
    cleanup_test_dir(test_dir);
    return 0;
}

int main(void) {
    TEST_INIT();
    
    RUN_TEST(test_wal_format);
    RUN_TEST(test_segment_ops);
    RUN_TEST(test_basic_rw);
    RUN_TEST(test_checksum);
    RUN_TEST(test_total_size);
    RUN_TEST(test_segment_cleanup);
    RUN_TEST(test_error_handling);
    
    TEST_SUMMARY();
    return 0;
} 