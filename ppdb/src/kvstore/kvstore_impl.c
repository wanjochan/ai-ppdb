#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "kvstore/internal/kvstore_types.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "kvstore/internal/kvstore_wal.h"
#include "kvstore/internal/kvstore_wal_types.h"
#include "kvstore/internal/sync.h"

// 工厂函数实现
ppdb_error_t ppdb_memtable_create(size_t size_limit, ppdb_memtable_t** table) {
    return ppdb_memtable_create_basic(size_limit, table);
}

ppdb_error_t ppdb_memtable_create_sharded(size_t size_limit, ppdb_memtable_t** table) {
    return ppdb_memtable_create_sharded_basic(size_limit, table);
}

void ppdb_memtable_destroy(ppdb_memtable_t* table) {
    switch (table->type) {
        case PPDB_MEMTABLE_BASIC:
            ppdb_memtable_destroy_basic(table);
            break;
        case PPDB_MEMTABLE_SHARDED:
            ppdb_memtable_destroy_sharded(table);
            break;
        case PPDB_MEMTABLE_LOCKFREE:
            ppdb_memtable_destroy_sharded(table);
            break;
    }
}

ppdb_error_t ppdb_memtable_put(ppdb_memtable_t* table,
                              const void* key, size_t key_len,
                              const void* value, size_t value_len) {
    if (!table) {
        return PPDB_ERR_INVALID_ARG;
    }

    switch (table->type) {
        case PPDB_MEMTABLE_BASIC:
            return ppdb_memtable_put_basic(table, key, key_len, value, value_len);
        case PPDB_MEMTABLE_SHARDED:
            return ppdb_memtable_put_sharded_basic(table, key, key_len, value, value_len);
        case PPDB_MEMTABLE_LOCKFREE:
            return ppdb_memtable_put_lockfree_basic(table, key, key_len, value, value_len);
        default:
            return PPDB_ERR_INVALID_ARG;
    }
}

ppdb_error_t ppdb_memtable_put_lockfree(ppdb_memtable_t* table,
                                       const void* key, size_t key_len,
                                       const void* value, size_t value_len) {
    return ppdb_memtable_put_lockfree_basic(table, key, key_len, value, value_len);
}

ppdb_error_t ppdb_memtable_get(ppdb_memtable_t* table,
                              const void* key, size_t key_len,
                              void** value, size_t* value_len) {
    if (!table) {
        return PPDB_ERR_INVALID_ARG;
    }

    switch (table->type) {
        case PPDB_MEMTABLE_BASIC:
            return ppdb_memtable_get_basic(table, key, key_len, value, value_len);
        case PPDB_MEMTABLE_SHARDED:
            return ppdb_memtable_get_sharded_basic(table, key, key_len, value, value_len);
        case PPDB_MEMTABLE_LOCKFREE:
            return ppdb_memtable_get_lockfree_basic(table, key, key_len, value, value_len);
        default:
            return PPDB_ERR_INVALID_ARG;
    }
}

ppdb_error_t ppdb_memtable_get_lockfree(ppdb_memtable_t* table,
                                       const void* key, size_t key_len,
                                       void** value, size_t* value_len) {
    return ppdb_memtable_get_lockfree_basic(table, key, key_len, value, value_len);
}

ppdb_error_t ppdb_memtable_delete(ppdb_memtable_t* table,
                                 const void* key, size_t key_len) {
    if (!table) {
        return PPDB_ERR_INVALID_ARG;
    }

    switch (table->type) {
        case PPDB_MEMTABLE_BASIC:
            return ppdb_memtable_delete_basic(table, key, key_len);
        case PPDB_MEMTABLE_SHARDED:
            return ppdb_memtable_delete_sharded_basic(table, key, key_len);
        case PPDB_MEMTABLE_LOCKFREE:
            return ppdb_memtable_delete_lockfree_basic(table, key, key_len);
        default:
            return PPDB_ERR_INVALID_ARG;
    }
}

ppdb_error_t ppdb_memtable_delete_lockfree(ppdb_memtable_t* table,
                                          const void* key, size_t key_len) {
    return ppdb_memtable_delete_lockfree_basic(table, key, key_len);
}

size_t ppdb_memtable_size(ppdb_memtable_t* table) {
    if (!table) {
        return 0;
    }

    switch (table->type) {
        case PPDB_MEMTABLE_BASIC:
            return ppdb_memtable_size_basic(table);
        case PPDB_MEMTABLE_SHARDED:
            return table->current_size;
        case PPDB_MEMTABLE_LOCKFREE:
            return table->current_size;
        default:
            return 0;
    }
}

size_t ppdb_memtable_max_size(ppdb_memtable_t* table) {
    if (!table) {
        return 0;
    }
    return table->size_limit;
}

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

// WAL工厂函数实现
ppdb_error_t ppdb_wal_create(const ppdb_wal_config_t* config, ppdb_wal_t** wal) {
    return ppdb_wal_create_basic(config, wal);
}

void ppdb_wal_destroy(ppdb_wal_t* wal) {
    ppdb_wal_destroy_basic(wal);
}

ppdb_error_t ppdb_wal_write(ppdb_wal_t* wal, const void* key, size_t key_len,
                           const void* value, size_t value_len) {
    return ppdb_wal_write_basic(wal, key, key_len, value, value_len);
}

ppdb_error_t ppdb_wal_write_lockfree(ppdb_wal_t* wal,
                                    const void* key, size_t key_len,
                                    const void* value, size_t value_len) {
    return ppdb_wal_write_lockfree_basic(wal, key, key_len, value, value_len);
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

ppdb_error_t ppdb_wal_recovery_iter_next(ppdb_wal_recovery_iter_t* iter,
                                        void** key, size_t* key_len,
                                        void** value, size_t* value_len) {
    return ppdb_wal_recovery_iter_next_basic(iter, key, key_len, value, value_len);
}

void ppdb_wal_recovery_iter_destroy(ppdb_wal_recovery_iter_t* iter) {
    ppdb_wal_recovery_iter_destroy_basic(iter);
} 