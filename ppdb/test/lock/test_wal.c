#include <cosmopolitan.h>
#include "ppdb/logger.h"
#include "src/kvstore/wal.h"

// 测试WAL文件系统操作
static bool test_fs_ops() {
    printf("Testing WAL filesystem operations...\n");
    
    // 创建WAL
    const char* path = "test_wal_fs.db";
    wal_t* wal = wal_create(path);
    if (!wal) {
        printf("Failed to create WAL at: %s\n", path);
        return false;
    }
    
    // 销毁WAL
    wal_destroy(wal);
    return true;
}

// 测试WAL写入操作
static bool test_write() {
    printf("Testing WAL write operations...\n");
    
    // 创建WAL
    const char* path = "test_wal_write.db";
    wal_t* wal = wal_create(path);
    if (!wal) {
        printf("Failed to create WAL at: %s\n", path);
        return false;
    }
    
    // 写入记录
    const char* key = "test_key";
    const char* value = "test_value";
    if (!wal_write(wal, WAL_PUT, key, strlen(key), value, strlen(value) + 1)) {
        printf("Failed to write WAL record\n");
        wal_destroy(wal);
        return false;
    }
    
    // 销毁WAL
    wal_destroy(wal);
    return true;
}

// 测试WAL恢复
static bool test_recovery() {
    printf("Testing WAL recovery...\n");
    
    // 创建WAL并写入记录
    const char* path = "test_wal_recovery.db";
    wal_t* wal = wal_create(path);
    if (!wal) {
        printf("Failed to create WAL at: %s\n", path);
        return false;
    }
    
    const char* key = "recovery_key";
    const char* value = "recovery_value";
    if (!wal_write(wal, WAL_PUT, key, strlen(key), value, strlen(value) + 1)) {
        printf("Failed to write WAL record\n");
        wal_destroy(wal);
        return false;
    }
    
    // 关闭WAL
    wal_close(wal);
    
    // 重新打开WAL并恢复
    wal = wal_create(path);
    if (!wal) {
        printf("Failed to reopen WAL at: %s\n", path);
        return false;
    }
    
    // 销毁WAL
    wal_destroy(wal);
    return true;
}

// WAL测试用例
static test_case_t wal_test_cases[] = {
    {"fs_ops", test_fs_ops},
    {"write", test_write},
    {"recovery", test_recovery}
};

// WAL测试套件
static test_suite_t wal_test_suite = {
    "WAL",
    wal_test_cases,
    sizeof(wal_test_cases) / sizeof(wal_test_cases[0])
};

int main() {
    printf("Starting WAL tests...\n");
    
    // 初始化日志系统
    ppdb_log_init(NULL);
    
    // 运行测试套件
    int result = run_test_suite(&wal_test_suite);
    
    printf("WAL tests completed with result: %d\n", result);
    return result;
} 