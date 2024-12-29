#ifndef PPDB_KVSTORE_WAL_H
#define PPDB_KVSTORE_WAL_H

#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_kvstore.h"
#include "kvstore_memtable.h"

// WAL记录类型
typedef enum {
    PPDB_WAL_RECORD_PUT,
    PPDB_WAL_RECORD_DELETE
} ppdb_wal_record_type_t;

// WAL结构
typedef struct ppdb_wal ppdb_wal_t;

// 创建WAL
ppdb_error_t ppdb_wal_create(const ppdb_wal_config_t* config, ppdb_wal_t** wal);
ppdb_error_t ppdb_wal_create_lockfree(const ppdb_wal_config_t* config, ppdb_wal_t** wal);

// 写入记录
ppdb_error_t ppdb_wal_write(ppdb_wal_t* wal, ppdb_wal_record_type_t type,
                           const void* key, size_t key_len,
                           const void* value, size_t value_len);

ppdb_error_t ppdb_wal_write_lockfree(ppdb_wal_t* wal, ppdb_wal_record_type_t type,
                                    const void* key, size_t key_len,
                                    const void* value, size_t value_len);

// 恢复操作
ppdb_error_t ppdb_wal_recover(ppdb_wal_t* wal, ppdb_memtable_t* table);
ppdb_error_t ppdb_wal_recover_lockfree(ppdb_wal_t* wal, ppdb_memtable_t* table);

// 关闭和销毁
void ppdb_wal_close(ppdb_wal_t* wal);
void ppdb_wal_destroy(ppdb_wal_t* wal);
void ppdb_wal_close_lockfree(ppdb_wal_t* wal);
void ppdb_wal_destroy_lockfree(ppdb_wal_t* wal);

#endif // PPDB_KVSTORE_WAL_H 