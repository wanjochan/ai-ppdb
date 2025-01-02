#ifndef PPDB_TYPES_H
#define PPDB_TYPES_H

#include <cosmopolitan.h>
#include "ppdb/sync.h"

// 前向声明
typedef struct ppdb_kvstore ppdb_kvstore_t;
typedef struct ppdb_memtable ppdb_memtable_t;
typedef struct ppdb_skiplist ppdb_skiplist_t;
typedef struct ppdb_wal ppdb_wal_t;

// 内存表类型
typedef enum ppdb_memtable_type {
    PPDB_MEMTABLE_BASIC,     // 基本内存表
    PPDB_MEMTABLE_SHARDED,   // 分片内存表
    PPDB_MEMTABLE_LOCKFREE   // 无锁内存表
} ppdb_memtable_type_t;

// 跳表节点
typedef struct ppdb_skiplist_node {
    void* key;                  // 键
    size_t key_len;            // 键长度
    void* value;               // 值
    size_t value_len;          // 值长度
    uint32_t level;            // 层数
    struct ppdb_skiplist_node** next;  // 后继节点数组
} ppdb_skiplist_node_t;

// 跳表
typedef struct ppdb_skiplist {
    ppdb_skiplist_node_t* head;  // 头节点
    uint32_t max_level;          // 最大层数
    uint32_t level;              // 当前层数
    size_t size;                 // 节点数量
    ppdb_sync_t* lock;          // 同步锁
} ppdb_skiplist_t;

// 内存表
typedef struct ppdb_memtable {
    ppdb_memtable_type_t type;  // 内存表类型
    ppdb_skiplist_t* skiplist;  // 跳表
    ppdb_sync_t* lock;          // 同步锁
    size_t size;                // 当前大小
    size_t max_size;            // 最大大小
} ppdb_memtable_t;

// KVStore配置
typedef struct ppdb_kvstore_config {
    ppdb_memtable_type_t type;  // 内存表类型
    size_t memtable_size;       // 内存表大小
    uint32_t max_level;         // 最大层数
    bool use_lockfree;          // 是否使用无锁模式
    bool enable_wal;            // 是否启用WAL
    char* wal_dir;              // WAL目录
} ppdb_kvstore_config_t;

// KVStore
typedef struct ppdb_kvstore {
    ppdb_kvstore_config_t config;  // 配置
    ppdb_memtable_t* memtable;     // 内存表
    ppdb_wal_t* wal;               // WAL
    ppdb_sync_t* lock;             // 同步锁
} ppdb_kvstore_t;

#endif // PPDB_TYPES_H