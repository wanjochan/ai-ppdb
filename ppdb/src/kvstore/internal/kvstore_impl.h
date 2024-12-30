#ifndef PPDB_KVSTORE_IMPL_H
#define PPDB_KVSTORE_IMPL_H

#include "ppdb/ppdb_error.h"
#include "kvstore/internal/kvstore_types.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "kvstore/internal/kvstore_wal.h"
#include "kvstore/internal/metrics.h"

// Basic memtable functions
ppdb_error_t ppdb_memtable_create_basic(size_t size, ppdb_memtable_t** table);
void ppdb_memtable_destroy_basic(ppdb_memtable_t* table);
ppdb_error_t ppdb_memtable_put_basic(ppdb_memtable_t* table,
                                    const void* key, size_t key_len,
                                    const void* value, size_t value_len);
ppdb_error_t ppdb_memtable_get_basic(ppdb_memtable_t* table,
                                    const void* key, size_t key_len,
                                    void** value, size_t* value_len);
ppdb_error_t ppdb_memtable_delete_basic(ppdb_memtable_t* table,
                                       const void* key, size_t key_len);
size_t ppdb_memtable_size_basic(ppdb_memtable_t* table);
size_t ppdb_memtable_max_size_basic(ppdb_memtable_t* table);
bool ppdb_memtable_is_immutable_basic(ppdb_memtable_t* table);
void ppdb_memtable_set_immutable_basic(ppdb_memtable_t* table);
const ppdb_metrics_t* ppdb_memtable_get_metrics_basic(ppdb_memtable_t* table);

// Basic sharded memtable functions
ppdb_error_t ppdb_memtable_create_sharded_basic(size_t size, ppdb_memtable_t** table);
ppdb_error_t ppdb_memtable_put_lockfree_basic(ppdb_memtable_t* table,
                                             const void* key, size_t key_len,
                                             const void* value, size_t value_len);
ppdb_error_t ppdb_memtable_get_lockfree_basic(ppdb_memtable_t* table,
                                             const void* key, size_t key_len,
                                             void** value, size_t* value_len);
ppdb_error_t ppdb_memtable_delete_lockfree_basic(ppdb_memtable_t* table,
                                                const void* key, size_t key_len);

// Basic memtable iterator functions
ppdb_error_t ppdb_memtable_iterator_create_basic(ppdb_memtable_t* table,
                                                ppdb_memtable_iterator_t** iter);
ppdb_error_t ppdb_memtable_iterator_next_basic(ppdb_memtable_iterator_t* iter,
                                              void** key, size_t* key_len,
                                              void** value, size_t* value_len);
void ppdb_memtable_iterator_destroy_basic(ppdb_memtable_iterator_t* iter);

// Factory functions for memtable iterator
ppdb_error_t ppdb_memtable_iterator_create(ppdb_memtable_t* table,
                                         ppdb_memtable_iterator_t** iter);
ppdb_error_t ppdb_memtable_iterator_next(ppdb_memtable_iterator_t* iter,
                                       void** key, size_t* key_len,
                                       void** value, size_t* value_len);
void ppdb_memtable_iterator_destroy(ppdb_memtable_iterator_t* iter);

// Factory functions for WAL
ppdb_error_t ppdb_wal_write_lockfree(ppdb_wal_t* wal,
                                   const void* key, size_t key_len,
                                   const void* value, size_t value_len);
ppdb_error_t ppdb_wal_recovery_iter_create(ppdb_wal_t* wal,
                                         ppdb_wal_recovery_iter_t** iter);
ppdb_error_t ppdb_wal_recovery_iter_next(ppdb_wal_recovery_iter_t* iter,
                                       void** key, size_t* key_len,
                                       void** value, size_t* value_len);
void ppdb_wal_recovery_iter_destroy(ppdb_wal_recovery_iter_t* iter);

#endif // PPDB_KVSTORE_IMPL_H 