#ifndef PPDB_INTERNAL_CORE_H
#define PPDB_INTERNAL_CORE_H

#include "../include/ppdb/ppdb.h"
#include "base.h"

//-----------------------------------------------------------------------------
// 内部类型定义
//-----------------------------------------------------------------------------

// 同步原语类型
typedef enum ppdb_sync_type {
    PPDB_SYNC_MUTEX,     // 互斥锁
    PPDB_SYNC_SPINLOCK,  // 自旋锁
    PPDB_SYNC_RWLOCK     // 读写锁
} ppdb_sync_type_t;

// 同步原语
typedef struct ppdb_sync {
    ppdb_sync_type_t type;
    union {
        struct {
            atomic_flag flag;
            uint32_t owner;
        } mutex;
        struct {
            atomic_flag flag;
            uint32_t count;
        } spinlock;
        struct {
            atomic_uint readers;
            atomic_flag writer;
        } rwlock;
    };
    struct {
        atomic_uint64_t contention_count;
        atomic_uint64_t wait_time_us;
    } stats;
} ppdb_sync_t;

// 内存页
typedef struct ppdb_page {
    uint32_t id;
    uint8_t* data;
    uint32_t size;
    bool dirty;
    ppdb_sync_t lock;
} ppdb_page_t;

// 事务上下文
typedef struct ppdb_tx_ctx {
    uint64_t id;
    bool read_only;
    ppdb_page_t** pages;
    uint32_t page_count;
    void* snapshot;
} ppdb_tx_ctx_t;

// 数据库上下文
typedef struct ppdb_db_ctx {
    char* name;
    ppdb_sync_t global_lock;
    ppdb_page_t** pages;
    uint32_t page_count;
    ppdb_tx_ctx_t** active_txs;
    uint32_t tx_count;
} ppdb_db_ctx_t;

// 全局上下文
typedef struct ppdb_ctx_impl {
    ppdb_options_t options;
    ppdb_sync_t lock;
    ppdb_db_ctx_t** dbs;
    uint32_t db_count;
} ppdb_ctx_impl_t;

//-----------------------------------------------------------------------------
// 内部函数
//-----------------------------------------------------------------------------

// 同步原语操作
ppdb_error_t ppdb_sync_init(ppdb_sync_t* sync, ppdb_sync_type_t type);
ppdb_error_t ppdb_sync_destroy(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_lock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_unlock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_rdlock(ppdb_sync_t* sync);
ppdb_error_t ppdb_sync_rdunlock(ppdb_sync_t* sync);

// 内存页操作
ppdb_error_t ppdb_page_create(ppdb_page_t** page, uint32_t size);
ppdb_error_t ppdb_page_destroy(ppdb_page_t* page);
ppdb_error_t ppdb_page_read(ppdb_page_t* page, uint32_t offset, void* data, uint32_t size);
ppdb_error_t ppdb_page_write(ppdb_page_t* page, uint32_t offset, const void* data, uint32_t size);

// 事务操作
ppdb_error_t ppdb_tx_create(ppdb_tx_ctx_t** tx, bool read_only);
ppdb_error_t ppdb_tx_destroy(ppdb_tx_ctx_t* tx);
ppdb_error_t ppdb_tx_add_page(ppdb_tx_ctx_t* tx, ppdb_page_t* page);
ppdb_error_t ppdb_tx_remove_page(ppdb_tx_ctx_t* tx, ppdb_page_t* page);

// 数据库操作
ppdb_error_t ppdb_db_create(ppdb_db_ctx_t** db, const char* name);
ppdb_error_t ppdb_db_destroy(ppdb_db_ctx_t* db);
ppdb_error_t ppdb_db_add_tx(ppdb_db_ctx_t* db, ppdb_tx_ctx_t* tx);
ppdb_error_t ppdb_db_remove_tx(ppdb_db_ctx_t* db, ppdb_tx_ctx_t* tx);

#endif // PPDB_INTERNAL_CORE_H
