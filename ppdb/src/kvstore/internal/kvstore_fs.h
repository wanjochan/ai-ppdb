#ifndef PPDB_KVSTORE_FS_H
#define PPDB_KVSTORE_FS_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_fs.h"
#include "ppdb/ppdb_types.h"
#include "kvstore/internal/kvstore_types.h"

// KVStore 特定的文件系统操作

// WAL文件操作
ppdb_error_t kvstore_fs_write_wal(const char* wal_dir, uint64_t seq, const void* data, size_t size);
ppdb_error_t kvstore_fs_read_wal(const char* wal_dir, uint64_t seq, void* data, size_t size, size_t* bytes_read);
ppdb_error_t kvstore_fs_list_wal(const char* wal_dir, uint64_t* seqs, size_t* count);
ppdb_error_t kvstore_fs_remove_wal(const char* wal_dir, uint64_t seq);

// SSTable文件操作
ppdb_error_t kvstore_fs_write_sst(const char* sst_dir, uint64_t level, uint64_t number, const void* data, size_t size);
ppdb_error_t kvstore_fs_read_sst(const char* sst_dir, uint64_t level, uint64_t number, void* data, size_t size, size_t* bytes_read);
ppdb_error_t kvstore_fs_list_sst(const char* sst_dir, uint64_t level, uint64_t* numbers, size_t* count);
ppdb_error_t kvstore_fs_remove_sst(const char* sst_dir, uint64_t level, uint64_t number);

// 目录管理
ppdb_error_t kvstore_fs_init_dirs(const char* db_path);
ppdb_error_t kvstore_fs_cleanup_dirs(const char* db_path);

#endif // PPDB_KVSTORE_FS_H 
