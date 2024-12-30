#ifndef PPDB_KVSTORE_WAL_H
#define PPDB_KVSTORE_WAL_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_kvstore.h"
#include "kvstore/internal/kvstore_types.h"
#include "kvstore/internal/sync.h"
#include "kvstore/internal/kvstore_wal_types.h"

// 基础WAL操作
ppdb_error_t ppdb_wal_create_basic(const ppdb_wal_config_t* config, ppdb_wal_t** wal);
void ppdb_wal_destroy_basic(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_write_basic(ppdb_wal_t* wal, const void* key, size_t key_len, const void* value, size_t value_len);
ppdb_error_t ppdb_wal_sync_basic(ppdb_wal_t* wal);
size_t ppdb_wal_size_basic(ppdb_wal_t* wal);
uint64_t ppdb_wal_next_sequence_basic(ppdb_wal_t* wal);

// 无锁WAL操作
ppdb_error_t ppdb_wal_write_lockfree_basic(ppdb_wal_t* wal, const void* key, size_t key_len, const void* value, size_t value_len);
ppdb_error_t ppdb_wal_sync_lockfree_basic(ppdb_wal_t* wal);
size_t ppdb_wal_size_lockfree_basic(ppdb_wal_t* wal);
uint64_t ppdb_wal_next_sequence_lockfree_basic(ppdb_wal_t* wal);

// WAL恢复操作
ppdb_error_t ppdb_wal_recover(ppdb_wal_t* wal, ppdb_memtable_t* memtable);
ppdb_error_t ppdb_wal_recover_basic(ppdb_wal_t* wal, ppdb_memtable_t* memtable);
ppdb_error_t ppdb_wal_recover_lockfree(ppdb_wal_t* wal, ppdb_memtable_t* memtable);
ppdb_error_t ppdb_wal_recover_lockfree_basic(ppdb_wal_t* wal, ppdb_memtable_t* memtable);

// WAL迭代器操作
ppdb_error_t ppdb_wal_recovery_iter_create(ppdb_wal_t* wal, ppdb_wal_recovery_iter_t** iter);
ppdb_error_t ppdb_wal_recovery_iter_next(ppdb_wal_recovery_iter_t* iter, void** key, size_t* key_len, void** value, size_t* value_len);
void ppdb_wal_recovery_iter_destroy(ppdb_wal_recovery_iter_t* iter);
ppdb_error_t ppdb_wal_recovery_iter_create_basic(ppdb_wal_t* wal, ppdb_wal_recovery_iter_t** iter);
ppdb_error_t ppdb_wal_recovery_iter_next_basic(ppdb_wal_recovery_iter_t* iter, void** key, size_t* key_len, void** value, size_t* value_len);
void ppdb_wal_recovery_iter_destroy_basic(ppdb_wal_recovery_iter_t* iter);

// 工厂函数
ppdb_error_t ppdb_wal_create(const ppdb_wal_config_t* config, ppdb_wal_t** wal);
void ppdb_wal_destroy(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_write(ppdb_wal_t* wal, const void* key, size_t key_len, const void* value, size_t value_len);
ppdb_error_t ppdb_wal_sync(ppdb_wal_t* wal);
size_t ppdb_wal_size(ppdb_wal_t* wal);
uint64_t ppdb_wal_next_sequence(ppdb_wal_t* wal);

#endif // PPDB_KVSTORE_WAL_H 