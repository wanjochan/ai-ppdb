#include <cosmopolitan.h>
#include "test_framework.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "kvstore/internal/sync.h"

// 测试互斥锁模式
static int test_mutex_mode(void) {
    ppdb_sync_t sync;
    ppdb_error_t err;
    
    // 初始化配置
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 1000,
        .use_lockfree = false,
        .stripe_count = 1,
        .backoff_us = 100,
        .enable_ref_count = false
    };
    
    // 初始化同步原语
    err = ppdb_sync_init(&sync, &config);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to init mutex: %s", ppdb_error_string(err));
        return 1;
    }
    
    // 测试加锁
    err = ppdb_sync_lock(&sync);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to lock mutex: %s", ppdb_error_string(err));
        ppdb_sync_destroy(&sync);
        return 1;
    }
    
    // 测试解锁
    err = ppdb_sync_unlock(&sync);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to unlock mutex: %s", ppdb_error_string(err));
        ppdb_sync_destroy(&sync);
        return 1;
    }
    
    // 测试尝试加锁
    bool locked = ppdb_sync_try_lock(&sync);
    if (!locked) {
        ppdb_log_error("Failed to try lock mutex");
        ppdb_sync_destroy(&sync);
        return 1;
    }
    
    // 再次解锁
    err = ppdb_sync_unlock(&sync);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to unlock mutex after try lock: %s", ppdb_error_string(err));
        ppdb_sync_destroy(&sync);
        return 1;
    }
    
    // 销毁同步原语
    err = ppdb_sync_destroy(&sync);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to destroy mutex: %s", ppdb_error_string(err));
        return 1;
    }
    
    return 0;
}

// 测试自旋锁模式
static int test_spinlock_mode(void) {
    ppdb_sync_t sync;
    ppdb_error_t err;
    
    // 初始化配置
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_SPINLOCK,
        .spin_count = 1000,
        .use_lockfree = false,
        .stripe_count = 1,
        .backoff_us = 100,
        .enable_ref_count = false
    };
    
    // 初始化同步原语
    err = ppdb_sync_init(&sync, &config);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to init spinlock: %s", ppdb_error_string(err));
        return 1;
    }
    
    // 测试加锁
    err = ppdb_sync_lock(&sync);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to lock spinlock: %s", ppdb_error_string(err));
        ppdb_sync_destroy(&sync);
        return 1;
    }
    
    // 测试解锁
    err = ppdb_sync_unlock(&sync);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to unlock spinlock: %s", ppdb_error_string(err));
        ppdb_sync_destroy(&sync);
        return 1;
    }
    
    // 测试尝试加锁
    bool locked = ppdb_sync_try_lock(&sync);
    if (!locked) {
        ppdb_log_error("Failed to try lock spinlock");
        ppdb_sync_destroy(&sync);
        return 1;
    }
    
    // 再次解锁
    err = ppdb_sync_unlock(&sync);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to unlock spinlock after try lock: %s", ppdb_error_string(err));
        ppdb_sync_destroy(&sync);
        return 1;
    }
    
    // 销毁同步原语
    err = ppdb_sync_destroy(&sync);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to destroy spinlock: %s", ppdb_error_string(err));
        return 1;
    }
    
    return 0;
}

// 测试读写锁模式
static int test_rwlock_mode(void) {
    ppdb_sync_t sync;
    ppdb_error_t err;
    
    // 初始化配置
    ppdb_sync_config_t config = {
        .type = PPDB_SYNC_RWLOCK,
        .spin_count = 1000,
        .use_lockfree = false,
        .stripe_count = 1,
        .backoff_us = 100,
        .enable_ref_count = false
    };
    
    // 初始化同步原语
    err = ppdb_sync_init(&sync, &config);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to init rwlock: %s", ppdb_error_string(err));
        return 1;
    }
    
    // 测试加写锁
    err = ppdb_sync_lock(&sync);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to write lock rwlock: %s", ppdb_error_string(err));
        ppdb_sync_destroy(&sync);
        return 1;
    }
    
    // 测试解锁
    err = ppdb_sync_unlock(&sync);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to unlock rwlock: %s", ppdb_error_string(err));
        ppdb_sync_destroy(&sync);
        return 1;
    }
    
    // 测试尝试加写锁
    bool locked = ppdb_sync_try_lock(&sync);
    if (!locked) {
        ppdb_log_error("Failed to try write lock rwlock");
        ppdb_sync_destroy(&sync);
        return 1;
    }
    
    // 再次解锁
    err = ppdb_sync_unlock(&sync);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to unlock rwlock after try lock: %s", ppdb_error_string(err));
        ppdb_sync_destroy(&sync);
        return 1;
    }
    
    // 销毁同步原语
    err = ppdb_sync_destroy(&sync);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to destroy rwlock: %s", ppdb_error_string(err));
        return 1;
    }
    
    return 0;
}

// 测试套件定义
static const test_case_t sync_test_cases[] = {
    {"test_mutex_mode", test_mutex_mode, 0, false, "Test mutex synchronization mode"},
    {"test_spinlock_mode", test_spinlock_mode, 0, false, "Test spinlock synchronization mode"},
    {"test_rwlock_mode", test_rwlock_mode, 0, false, "Test read-write lock synchronization mode"}
};

static const test_suite_t sync_test_suite = {
    .name = "Sync Primitive Tests",
    .cases = sync_test_cases,
    .num_cases = sizeof(sync_test_cases) / sizeof(test_case_t),
    .setup = NULL,
    .teardown = NULL
};

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    // 初始化测试框架
    test_framework_init();
    
    // 运行测试套件
    int failed = run_test_suite(&sync_test_suite);
    
    // 打印测试统计
    test_print_stats();
    
    // 清理测试框架
    test_framework_cleanup();
    
    return failed ? 1 : 0;
} 