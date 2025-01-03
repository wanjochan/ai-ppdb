#include <cosmopolitan.h>
#include "ppdb/ppdb.h"
#include "ppdb/ppdb_kvstore.h"

int main(int argc, char* argv[]) {
    // 初始化配置
    ppdb_kvstore_config_t config = {
        .data_dir = "data",
        .memtable_size = 64 * 1024 * 1024,  // 64MB
        .use_sharding = false,
        .adaptive_sharding = true,
        .enable_compression = true,
        .enable_monitoring = true,
        .wal = {
            .dir_path = "data/wal",
            .segment_size = 4 * 1024 * 1024,  // 4MB
            .sync_write = true,
            .enable_compression = true,
            .buffer_size = 1 * 1024 * 1024   // 1MB
        }
    };

    // 创建KVStore
    ppdb_kvstore_t* store = NULL;
    ppdb_error_t err = ppdb_kvstore_create(&config, &store);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to create KVStore: %s\n", ppdb_error_string(err));
        return 1;
    }

    // TODO: 添加命令行参数处理和交互式界面

    // 清理资源
    ppdb_kvstore_destroy(store);
    return 0;
}