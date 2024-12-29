#ifndef PPDB_WAL_UNIFIED_H
#define PPDB_WAL_UNIFIED_H

#include <stdint.h>
#include <stdbool.h>
#include "../common/sync_unified.h"

// WAL记录类型
typedef enum ppdb_wal_record_type {
    WAL_RECORD_PUT = 1,
    WAL_RECORD_DELETE = 2,
    WAL_RECORD_CHECKPOINT = 3
} ppdb_wal_record_type_t;

// WAL配置
typedef struct ppdb_wal_config {
    ppdb_sync_config_t sync_config;     // 同步配置
    size_t buffer_size;                 // 写缓冲区大小
    bool enable_group_commit;           // 是否启用组提交
    uint32_t group_commit_interval;     // 组提交间隔(ms)
    bool enable_async_flush;            // 是否启用异步刷盘
    bool enable_checksum;               // 是否启用校验和
} ppdb_wal_config_t;

// WAL结构
typedef struct ppdb_wal {
    int fd;                            // 文件描述符
    char* filename;                    // 文件名
    uint64_t file_size;               // 文件大小
    
    // 配置
    ppdb_wal_config_t config;
    
    // 同步机制
    ppdb_sync_t sync;
    
    // 写缓冲
    struct {
        void* buffer;                  // 写缓冲区
        size_t size;                   // 缓冲区大小
        size_t used;                   // 已使用大小
        ppdb_sync_t buffer_lock;       // 缓冲区锁
    } write_buffer;
    
    // 组提交
    struct {
        bool enabled;                  // 是否启用
        uint32_t interval;             // 提交间隔
        uint64_t last_commit;          // 上次提交时间
        void* thread;                  // 提交线程
    } group_commit;
    
    // 统计信息
    struct {
        atomic_uint64_t total_writes;   // 总写入次数
        atomic_uint64_t sync_writes;    // 同步写入次数
        atomic_uint64_t bytes_written;  // 写入字节数
        atomic_uint64_t flush_count;    // 刷盘次数
    } stats;
    
    // 状态标志
    atomic_bool is_closing;            // 是否正在关闭
} ppdb_wal_t;

// WAL记录头
typedef struct ppdb_wal_record_header {
    uint32_t type;                     // 记录类型
    uint32_t key_size;                 // 键大小
    uint32_t value_size;               // 值大小
    uint64_t sequence;                 // 序列号
    uint32_t checksum;                 // 校验和
} ppdb_wal_record_header_t;

// API函数
ppdb_wal_t* ppdb_wal_create(const char* filename, 
                           const ppdb_wal_config_t* config);
void ppdb_wal_destroy(ppdb_wal_t* wal);

int ppdb_wal_append(ppdb_wal_t* wal,
                    ppdb_wal_record_type_t type,
                    const void* key, size_t key_size,
                    const void* value, size_t value_size,
                    uint64_t sequence);

int ppdb_wal_sync(ppdb_wal_t* wal);
int ppdb_wal_flush(ppdb_wal_t* wal);

// 恢复相关
typedef struct ppdb_wal_recovery_iter {
    ppdb_wal_t* wal;
    uint64_t offset;
    void* buffer;
    size_t buffer_size;
} ppdb_wal_recovery_iter_t;

ppdb_wal_recovery_iter_t* ppdb_wal_recovery_iter_create(ppdb_wal_t* wal);
void ppdb_wal_recovery_iter_destroy(ppdb_wal_recovery_iter_t* iter);
bool ppdb_wal_recovery_iter_valid(ppdb_wal_recovery_iter_t* iter);
int ppdb_wal_recovery_iter_next(ppdb_wal_recovery_iter_t* iter,
                               ppdb_wal_record_type_t* type,
                               void** key, size_t* key_size,
                               void** value, size_t* value_size,
                               uint64_t* sequence);

#endif // PPDB_WAL_UNIFIED_H
