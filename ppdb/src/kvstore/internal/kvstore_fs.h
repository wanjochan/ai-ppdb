#ifndef PPDB_KVSTORE_FS_H
#define PPDB_KVSTORE_FS_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_fs.h"
#include "kvstore/internal/sync.h"

// 内部文件系统操作函数声明

// 写入文件
ppdb_error_t ppdb_write_file(const char* filename, const void* data, size_t size);

// 读取文件
ppdb_error_t ppdb_read_file(const char* filename, void* data, size_t size, size_t* bytes_read);

// 追加文件
ppdb_error_t ppdb_append_file(const char* filename, const void* data, size_t size);

// 获取文件大小
ppdb_error_t ppdb_get_file_size(const char* filename, size_t* size);

// 同步文件
ppdb_error_t ppdb_sync_file(const char* filename);

#endif // PPDB_KVSTORE_FS_H 
