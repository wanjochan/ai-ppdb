#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "kvstore/internal/kvstore_types.h"
#include "kvstore/internal/kvstore_memtable.h"

// 创建基础内存表
extern ppdb_error_t ppdb_memtable_create_basic(size_t size_limit, ppdb_memtable_t** table);

// 创建分片内存表
extern ppdb_error_t ppdb_memtable_create_sharded_basic(size_t size_limit, ppdb_memtable_t** table);

// 工厂函数 - 创建分片内存表
ppdb_error_t ppdb_memtable_create_sharded(size_t size_limit, ppdb_memtable_t** table) {
    return ppdb_memtable_create_sharded_basic(size_limit, table);
}

// 工厂函数 - 创建无锁内存表
ppdb_error_t ppdb_memtable_create_lockfree(size_t size_limit, ppdb_memtable_t** table) {
    ppdb_error_t err = ppdb_memtable_create_sharded_basic(size_limit, table);
    if (err == PPDB_OK && *table) {
        (*table)->type = PPDB_MEMTABLE_LOCKFREE;
    }
    return err;
}

// 工厂函数 - 创建内存表
ppdb_error_t ppdb_memtable_create(size_t size_limit, ppdb_memtable_t** table) {
    return ppdb_memtable_create_basic(size_limit, table);
} 