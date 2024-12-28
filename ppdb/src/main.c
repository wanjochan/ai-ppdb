#include <cosmopolitan.h>
#include "ppdb/memtable.h"
#include "ppdb/wal.h"
#include "ppdb/logger.h"
#include "ppdb/kvstore.h"

// 获取运行模式
static ppdb_mode_t get_run_mode(int argc, char* argv[]) {
    // 1. 检查命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            if (strcmp(argv[i + 1], "lockfree") == 0) {
                return PPDB_MODE_LOCKFREE;
            }
            if (strcmp(argv[i + 1], "locked") == 0) {
                return PPDB_MODE_LOCKED;
            }
            break;
        }
    }

    // 2. 检查环境变量
    const char* env_mode = getenv("PPDB_MODE");
    if (env_mode) {
        if (strcmp(env_mode, "lockfree") == 0) {
            return PPDB_MODE_LOCKFREE;
        }
        if (strcmp(env_mode, "locked") == 0) {
            return PPDB_MODE_LOCKED;
        }
    }

    // 3. 使用默认值
    return PPDB_MODE_LOCKED;
}

int main(int argc, char* argv[]) {
    ppdb_log_info("PPDB starting...");

    // 获取运行模式
    ppdb_mode_t mode = get_run_mode(argc, argv);
    ppdb_log_info("Running in %s mode", mode == PPDB_MODE_LOCKFREE ? "lock-free" : "locked");

    // 创建KVStore配置
    ppdb_kvstore_config_t config = {
        .dir_path = "db",
        .memtable_size = 1024 * 1024,  // 1MB
        .l0_size = 1024 * 1024,        // 1MB
        .mode = mode
    };

    // 创建KVStore
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_create(&config, &store);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create KVStore: %d", err);
        return 1;
    }

    ppdb_log_info("PPDB started successfully");

    // 这里可以添加命令行交互逻辑
    // ...

    // 清理资源
    ppdb_kvstore_destroy(store);

    return 0;
}