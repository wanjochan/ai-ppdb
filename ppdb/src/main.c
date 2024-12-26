#include <cosmopolitan.h>
#include "ppdb/memtable.h"
#include "ppdb/wal.h"
#include "common/logger.h"

int main(int argc, char* argv[]) {
    ppdb_log_info("PPDB starting...");

    // 创建MemTable
    ppdb_memtable_t* memtable = NULL;
    ppdb_error_t err = ppdb_memtable_create(1024 * 1024, &memtable);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create memtable: %d", err);
        return 1;
    }

    // 创建WAL配置
    ppdb_wal_config_t wal_config = {
        .dir_path = "wal",
        .segment_size = 1024 * 1024,  // 1MB
        .sync_write = true
    };

    // 创建WAL
    ppdb_wal_t* wal = NULL;
    err = ppdb_wal_create(&wal_config, &wal);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create WAL: %d", err);
        ppdb_memtable_destroy(memtable);
        return 1;
    }

    // 从WAL恢复数据
    err = ppdb_wal_recover(wal, memtable);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to recover from WAL: %d", err);
        ppdb_wal_destroy(wal);
        ppdb_memtable_destroy(memtable);
        return 1;
    }

    ppdb_log_info("PPDB started successfully");

    // 这里可以添加命令行交互逻辑
    // ...

    // 清理资源
    ppdb_wal_destroy(wal);
    ppdb_memtable_destroy(memtable);

    return 0;
} 