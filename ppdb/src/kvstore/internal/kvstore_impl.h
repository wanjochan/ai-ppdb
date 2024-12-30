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

// Basic WAL functions
ppdb_error_t ppdb_wal_create_basic(const ppdb_wal_config_t* config, ppdb_wal_t** wal);
void ppdb_wal_destroy_basic(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_write_basic(ppdb_wal_t* wal, ppdb_wal_record_type_t type,
                                 const void* key, size_t key_len,
                                 const void* value, size_t value_len);
ppdb_error_t ppdb_wal_write_lockfree_basic(ppdb_wal_t* wal, ppdb_wal_record_type_t type,
                                          const void* key, size_t key_len,
                                          const void* value, size_t value_len);
ppdb_error_t ppdb_wal_sync_basic(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_sync_lockfree_basic(ppdb_wal_t* wal);
size_t ppdb_wal_size_basic(ppdb_wal_t* wal);
size_t ppdb_wal_size_lockfree_basic(ppdb_wal_t* wal);
uint64_t ppdb_wal_next_sequence_basic(ppdb_wal_t* wal);
uint64_t ppdb_wal_next_sequence_lockfree_basic(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_recover_basic(ppdb_wal_t* wal, ppdb_memtable_t* memtable);
ppdb_error_t ppdb_wal_recover_lockfree_basic(ppdb_wal_t* wal, ppdb_memtable_t* memtable);
ppdb_error_t ppdb_wal_recovery_iter_create_basic(ppdb_wal_t* wal,
                                                ppdb_wal_recovery_iter_t** iter);
void ppdb_wal_recovery_iter_destroy_basic(ppdb_wal_recovery_iter_t* iter);
ppdb_error_t ppdb_wal_recovery_iter_next_basic(ppdb_wal_recovery_iter_t* iter,
                                              ppdb_wal_record_type_t* type,
                                              void** key, size_t* key_len,
                                              void** value, size_t* value_len);

#endif // PPDB_KVSTORE_IMPL_H 