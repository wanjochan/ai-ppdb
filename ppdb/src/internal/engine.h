#ifndef PPDB_INTERNAL_ENGINE_H
#define PPDB_INTERNAL_ENGINE_H

#include <cosmopolitan.h>
#include "internal/base.h"
#include "base.h"

// Forward declarations
typedef struct ppdb_engine_s ppdb_engine_t;
typedef struct ppdb_engine_txn_s ppdb_engine_txn_t;

// Statistics
typedef struct ppdb_engine_stats_s {
    ppdb_base_counter_t* total_txns;
    ppdb_base_counter_t* active_txns;
    ppdb_base_counter_t* total_reads;
    ppdb_base_counter_t* total_writes;
} ppdb_engine_stats_t;

typedef struct ppdb_engine_txn_stats_s {
    ppdb_base_counter_t* reads;
    ppdb_base_counter_t* writes;
    bool is_active;
    bool is_committed;
    bool is_rolledback;
} ppdb_engine_txn_stats_t;

// Engine layer error codes (0x1200-0x12FF)
#define PPDB_ENGINE_ERR_INIT       (PPDB_ENGINE_ERR_START + 0x001)
#define PPDB_ENGINE_ERR_PARAM      (PPDB_ENGINE_ERR_START + 0x002)
#define PPDB_ENGINE_ERR_MUTEX      (PPDB_ENGINE_ERR_START + 0x003)
#define PPDB_ENGINE_ERR_TXN        (PPDB_ENGINE_ERR_START + 0x004)
#define PPDB_ENGINE_ERR_MVCC       (PPDB_ENGINE_ERR_START + 0x005)
#define PPDB_ENGINE_ERR_ASYNC      (PPDB_ENGINE_ERR_START + 0x006)
#define PPDB_ENGINE_ERR_TIMEOUT    (PPDB_ENGINE_ERR_START + 0x007)
#define PPDB_ENGINE_ERR_BUSY       (PPDB_ENGINE_ERR_START + 0x008)
#define PPDB_ENGINE_ERR_FULL       (PPDB_ENGINE_ERR_START + 0x009)
#define PPDB_ENGINE_ERR_NOT_FOUND  (PPDB_ENGINE_ERR_START + 0x00A)
#define PPDB_ENGINE_ERR_EXISTS     (PPDB_ENGINE_ERR_START + 0x00B)
#define PPDB_ENGINE_ERR_INVALID_STATE (PPDB_ENGINE_ERR_START + 0x00C)

// Error message conversion function
const char* ppdb_engine_strerror(ppdb_error_t err);

// Engine type
struct ppdb_engine_s {
    ppdb_base_t* base;                // Using ppdb_base_t from base.h
    ppdb_base_mutex_t* global_mutex;  // Using ppdb_base_mutex_t from base.h

    // 事务管理
    struct {
        ppdb_base_mutex_t* txn_mutex;
        uint64_t next_txn_id;
        ppdb_engine_txn_t* active_txns;
    } txn_mgr;

    // IO管理
    struct {
        ppdb_base_io_manager_t* io_mgr;
        ppdb_base_thread_t* io_thread;
        bool io_running;
    } io_mgr;

    // 统计信息
    ppdb_engine_stats_t stats;
};

// Engine-specific function declarations
ppdb_error_t ppdb_engine_init(ppdb_engine_t** engine, ppdb_base_t* base);
void ppdb_engine_destroy(ppdb_engine_t* engine);

// Transaction management
ppdb_error_t ppdb_engine_txn_init(ppdb_engine_t* engine);
void ppdb_engine_txn_cleanup(ppdb_engine_t* engine);
ppdb_error_t ppdb_engine_txn_begin(ppdb_engine_t* engine, ppdb_engine_txn_t** txn);
ppdb_error_t ppdb_engine_txn_commit(ppdb_engine_txn_t* txn);
ppdb_error_t ppdb_engine_txn_rollback(ppdb_engine_txn_t* txn);

// IO management
ppdb_error_t ppdb_engine_io_init(ppdb_engine_t* engine);
void ppdb_engine_io_cleanup(ppdb_engine_t* engine);

// Statistics functions
void ppdb_engine_get_stats(ppdb_engine_t* engine, ppdb_engine_stats_t* stats);
void ppdb_engine_txn_get_stats(ppdb_engine_txn_t* txn, ppdb_engine_txn_stats_t* stats);

// Statistics initialization and cleanup
ppdb_error_t ppdb_engine_stats_init(ppdb_engine_stats_t* stats);
void ppdb_engine_stats_cleanup(ppdb_engine_stats_t* stats);

#endif // PPDB_INTERNAL_ENGINE_H