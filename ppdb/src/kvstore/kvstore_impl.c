#include <cosmopolitan.h>

// 公共头文件
#include "ppdb/ppdb_error.h"

// 内部头文件
#include "kvstore/internal/kvstore_types.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "kvstore/internal/kvstore_wal.h"
#include "kvstore/internal/kvstore_logger.h"
#include "kvstore/internal/skiplist.h"
#include "kvstore/internal/sync.h"
#include "kvstore/internal/metrics.h"
#include "kvstore/internal/kvstore_impl.h"

// 导出的 memtable 函数
ppdb_error_t ppdb_memtable_create(size_t size, ppdb_memtable_t** table) {
    return ppdb_memtable_create_basic(size, table);
}

void ppdb_memtable_destroy(ppdb_memtable_t* table) {
    ppdb_memtable_destroy_basic(table);
}

ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* table,
                              const void* key, size_t key_len,
                              const void* value, size_t value_len) {
    return ppdb_memtable_put_basic(table, key, key_len, value, value_len);
}

ppdb_error_t ppdb_memtable_get(ppdb_memtable_t* table,
                              const void* key, size_t key_len,
                              void** value, size_t* value_len) {
    return ppdb_memtable_get_basic(table, key, key_len, value, value_len);
}

ppdb_error_t ppdb_memtable_delete(ppdb_memtable_t* table,
                                 const void* key, size_t key_len) {
    return ppdb_memtable_delete_basic(table, key, key_len);
}

size_t ppdb_memtable_size(ppdb_memtable_t* table) {
    return ppdb_memtable_size_basic(table);
}

size_t ppdb_memtable_max_size(ppdb_memtable_t* table) {
    return ppdb_memtable_max_size_basic(table);
}

bool ppdb_memtable_is_immutable(ppdb_memtable_t* table) {
    return ppdb_memtable_is_immutable_basic(table);
}

void ppdb_memtable_set_immutable(ppdb_memtable_t* table) {
    ppdb_memtable_set_immutable_basic(table);
}

const ppdb_metrics_t* ppdb_memtable_get_metrics(ppdb_memtable_t* table) {
    return ppdb_memtable_get_metrics_basic(table);
}

// 导出的 sharded memtable 函数
ppdb_error_t ppdb_memtable_create_sharded(size_t size, ppdb_memtable_t** table) {
    return ppdb_memtable_create_sharded_basic(size, table);
}

ppdb_error_t ppdb_memtable_create_lockfree(size_t size, ppdb_memtable_t** table) {
    ppdb_error_t err = ppdb_memtable_create_sharded_basic(size, table);
    if (err == PPDB_OK) {
        (*table)->config.use_lockfree = true;
    }
    return err;
}

void ppdb_memtable_destroy_lockfree(ppdb_memtable_t* table) {
    ppdb_memtable_destroy_basic(table);
}

ppdb_error_t ppdb_memtable_put_lockfree(ppdb_memtable_t* table,
                                       const void* key, size_t key_len,
                                       const void* value, size_t value_len) {
    return ppdb_memtable_put_lockfree_basic(table, key, key_len, value, value_len);
}

ppdb_error_t ppdb_memtable_get_lockfree(ppdb_memtable_t* table,
                                       const void* key, size_t key_len,
                                       void** value, size_t* value_len) {
    return ppdb_memtable_get_lockfree_basic(table, key, key_len, value, value_len);
}

ppdb_error_t ppdb_memtable_delete_lockfree(ppdb_memtable_t* table,
                                          const void* key, size_t key_len) {
    return ppdb_memtable_delete_lockfree_basic(table, key, key_len);
}

// 导出的 memtable iterator 函数
ppdb_error_t ppdb_memtable_iterator_create(ppdb_memtable_t* table,
                                          ppdb_memtable_iterator_t** iter) {
    return ppdb_memtable_iterator_create_basic(table, iter);
}

ppdb_error_t ppdb_memtable_iterator_next(ppdb_memtable_iterator_t* iter,
                                        void** key, size_t* key_len,
                                        void** value, size_t* value_len) {
    return ppdb_memtable_iterator_next_basic(iter, key, key_len, value, value_len);
}

void ppdb_memtable_iterator_destroy(ppdb_memtable_iterator_t* iter) {
    ppdb_memtable_iterator_destroy_basic(iter);
}

// 导出的 WAL 函数
ppdb_error_t ppdb_wal_create(const ppdb_wal_config_t* config, ppdb_wal_t** wal) {
    return ppdb_wal_create_basic(config, wal);
}

void ppdb_wal_destroy(ppdb_wal_t* wal) {
    ppdb_wal_destroy_basic(wal);
}

ppdb_error_t ppdb_wal_write(ppdb_wal_t* wal, ppdb_wal_record_type_t type,
                           const void* key, size_t key_len,
                           const void* value, size_t value_len) {
    return ppdb_wal_write_basic(wal, type, key, key_len, value, value_len);
}

ppdb_error_t ppdb_wal_write_lockfree(ppdb_wal_t* wal, ppdb_wal_record_type_t type,
                                    const void* key, size_t key_len,
                                    const void* value, size_t value_len) {
    return ppdb_wal_write_lockfree_basic(wal, type, key, key_len, value, value_len);
}

ppdb_error_t ppdb_wal_sync(ppdb_wal_t* wal) {
    return ppdb_wal_sync_basic(wal);
}

ppdb_error_t ppdb_wal_sync_lockfree(ppdb_wal_t* wal) {
    return ppdb_wal_sync_lockfree_basic(wal);
}

size_t ppdb_wal_size(ppdb_wal_t* wal) {
    return ppdb_wal_size_basic(wal);
}

size_t ppdb_wal_size_lockfree(ppdb_wal_t* wal) {
    return ppdb_wal_size_lockfree_basic(wal);
}

uint64_t ppdb_wal_next_sequence(ppdb_wal_t* wal) {
    return ppdb_wal_next_sequence_basic(wal);
}

uint64_t ppdb_wal_next_sequence_lockfree(ppdb_wal_t* wal) {
    return ppdb_wal_next_sequence_lockfree_basic(wal);
}

ppdb_error_t ppdb_wal_recover(ppdb_wal_t* wal, ppdb_memtable_t* memtable) {
    return ppdb_wal_recover_basic(wal, memtable);
}

ppdb_error_t ppdb_wal_recover_lockfree(ppdb_wal_t* wal, ppdb_memtable_t* memtable) {
    return ppdb_wal_recover_lockfree_basic(wal, memtable);
}

ppdb_error_t ppdb_wal_recovery_iter_create(ppdb_wal_t* wal,
                                          ppdb_wal_recovery_iter_t** iter) {
    return ppdb_wal_recovery_iter_create_basic(wal, iter);
}

void ppdb_wal_recovery_iter_destroy(ppdb_wal_recovery_iter_t* iter) {
    ppdb_wal_recovery_iter_destroy_basic(iter);
}

ppdb_error_t ppdb_wal_recovery_iter_next(ppdb_wal_recovery_iter_t* iter,
                                        ppdb_wal_record_type_t* type,
                                        void** key, size_t* key_len,
                                        void** value, size_t* value_len) {
    return ppdb_wal_recovery_iter_next_basic(iter, type, key, key_len, value, value_len);
} 