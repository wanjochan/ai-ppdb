#ifndef PPDB_KVSTORE_WAL_H
#define PPDB_KVSTORE_WAL_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_wal.h"
#include "kvstore/internal/kvstore_wal_types.h"

// WAL 基本操作（内部实现）
ppdb_error_t ppdb_wal_create_basic(const ppdb_wal_config_t* config, ppdb_wal_t** wal);
void ppdb_wal_destroy_basic(ppdb_wal_t* wal);
ppdb_error_t ppdb_wal_write_basic(ppdb_wal_t* wal, const void* key, size_t key_size,
                                 const void* value, size_t value_size);

// CRC32 计算函数
uint32_t calculate_crc32(const void* data, size_t size);
uint32_t calculate_crc32_update(uint32_t crc, const void* data, size_t size);

// 段文件名生成函数
char* generate_segment_filename(const char* dir_path, uint64_t segment_id);

#endif // PPDB_KVSTORE_WAL_H 