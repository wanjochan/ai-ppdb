#ifndef PPDB_SYNC_H
#define PPDB_SYNC_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"

// 同步原语操作函数声明

// 初始化同步原语
ppdb_error_t ppdb_sync_init(ppdb_sync_t* sync, const ppdb_sync_config_t* config);

// 销毁同步原语
ppdb_error_t ppdb_sync_destroy(ppdb_sync_t* sync);

// 加锁
ppdb_error_t ppdb_sync_lock(ppdb_sync_t* sync);

// 尝试加锁
bool ppdb_sync_try_lock(ppdb_sync_t* sync);

// 解锁
ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync);

// 同步文件
ppdb_error_t ppdb_sync_file(const char* filename);

// 同步文件描述符
ppdb_error_t ppdb_sync_fd(int fd);

// 哈希函数
uint32_t ppdb_sync_hash(const void* data, size_t len);

#endif // PPDB_SYNC_H
