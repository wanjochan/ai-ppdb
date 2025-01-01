#ifndef PPDB_KVSTORE_WAL_H
#define PPDB_KVSTORE_WAL_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_wal.h"
#include "kvstore/internal/kvstore_types.h"
#include "kvstore/internal/sync.h"
#include "kvstore/internal/kvstore_wal_types.h"

// 基础 WAL 操作
ppdb_error_t ppdb_wal_create_basic(const ppdb_wal_config_t* config, ppdb_wal_t** wal);
void ppdb_wal_destroy_basic(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_write_basic(ppdb_wal_t* wal, const void* key, size_t key_len,
                                const void* value, size_t value_len);
ppdb_error_t ppdb_wal_write_batch_basic(ppdb_wal_t* wal, const ppdb_write_batch_t* batch);
ppdb_error_t ppdb_wal_sync_basic(ppdb_wal_t* wal);
size_t ppdb_wal_size_basic(ppdb_wal_t* wal);
uint64_t ppdb_wal_next_sequence_basic(ppdb_wal_t* wal);

// 无锁操作
ppdb_error_t ppdb_wal_write_lockfree_basic(ppdb_wal_t* wal, const void* key, size_t key_len,
                                         const void* value, size_t value_len);
ppdb_error_t ppdb_wal_write_batch_lockfree_basic(ppdb_wal_t* wal,
                                               const ppdb_write_batch_t* batch);
ppdb_error_t ppdb_wal_sync_lockfree_basic(ppdb_wal_t* wal);
size_t ppdb_wal_size_lockfree_basic(ppdb_wal_t* wal);
uint64_t ppdb_wal_next_sequence_lockfree_basic(ppdb_wal_t* wal);

// 维护操作
ppdb_error_t ppdb_wal_cleanup_basic(ppdb_wal_t* wal, uint64_t min_sequence);
ppdb_error_t ppdb_wal_compact_basic(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_stats_basic(ppdb_wal_t* wal, ppdb_wal_stats_t* stats);
ppdb_error_t ppdb_wal_get_segment_info_basic(ppdb_wal_t* wal, size_t index,
                                           ppdb_wal_segment_info_t* info);
ppdb_error_t ppdb_wal_get_recovery_point_basic(ppdb_wal_t* wal,
                                             ppdb_wal_recovery_point_t* recovery_point);

// 迭代器操作
ppdb_error_t ppdb_wal_iterator_create_basic(ppdb_wal_t* wal,
                                         ppdb_wal_iterator_t** iterator);
void ppdb_wal_iterator_destroy_basic(ppdb_wal_iterator_t* iterator);
bool ppdb_wal_iterator_valid_basic(ppdb_wal_iterator_t* iterator);
ppdb_error_t ppdb_wal_iterator_next_basic(ppdb_wal_iterator_t* iterator);
ppdb_error_t ppdb_wal_iterator_get_basic(ppdb_wal_iterator_t* iterator,
                                      void** key, size_t* key_size,
                                      void** value, size_t* value_size);
uint64_t ppdb_wal_iterator_sequence_basic(ppdb_wal_iterator_t* iterator);
ppdb_error_t ppdb_wal_iterator_reset_basic(ppdb_wal_iterator_t* iterator);
ppdb_error_t ppdb_wal_iterator_seek_basic(ppdb_wal_iterator_t* iterator,
                                       uint64_t sequence);

// 内部工具函数
uint32_t calculate_crc32(const void* data, size_t size);
ppdb_error_t roll_new_segment(ppdb_wal_t* wal);
ppdb_error_t validate_segment(wal_segment_t* segment);
char* generate_segment_filename(const char* dir_path, uint64_t segment_id);

#endif // PPDB_KVSTORE_WAL_H