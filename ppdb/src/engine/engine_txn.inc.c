/*
 * engine_txn.inc.c - 引擎层事务管理实现
 */

#include <cosmopolitan.h>
#include "internal/engine.h"

// Transaction structure
typedef struct ppdb_engine_txn_s {
    uint64_t txn_id;                  // 事务ID
    ppdb_engine_t* engine;            // 所属引擎
    struct ppdb_engine_txn_s* next;   // 活跃事务链表
    bool is_active;                   // 事务是否活跃
    
    // 事务状态
    struct {
        atomic_bool is_committed;      // 是否已提交
        atomic_bool is_rolledback;     // 是否已回滚
    } state;
    
    // 事务统计
    struct {
        atomic_uint64_t reads;         // 读操作数
        atomic_uint64_t writes;        // 写操作数
    } stats;
} ppdb_engine_txn_t;

// Initialize transaction management
ppdb_error_t ppdb_engine_txn_init(ppdb_engine_t* engine) {
    ppdb_error_t err;

    if (!engine) return PPDB_ENGINE_ERR_PARAM;

    // 创建事务统计计数器
    err = ppdb_base_counter_create(&engine->stats.total_txns);
    if (err != PPDB_OK) return err;

    err = ppdb_base_counter_create(&engine->stats.active_txns);
    if (err != PPDB_OK) {
        ppdb_base_counter_destroy(engine->stats.total_txns);
        return err;
    }

    err = ppdb_base_counter_create(&engine->stats.total_reads);
    if (err != PPDB_OK) {
        ppdb_base_counter_destroy(engine->stats.total_txns);
        ppdb_base_counter_destroy(engine->stats.active_txns);
        return err;
    }

    err = ppdb_base_counter_create(&engine->stats.total_writes);
    if (err != PPDB_OK) {
        ppdb_base_counter_destroy(engine->stats.total_txns);
        ppdb_base_counter_destroy(engine->stats.active_txns);
        ppdb_base_counter_destroy(engine->stats.total_reads);
        return err;
    }

    // 创建事务互斥锁
    err = ppdb_base_mutex_create(&engine->txn_mutex);
    if (err != PPDB_OK) {
        ppdb_base_counter_destroy(engine->stats.total_txns);
        ppdb_base_counter_destroy(engine->stats.active_txns);
        ppdb_base_counter_destroy(engine->stats.total_reads);
        ppdb_base_counter_destroy(engine->stats.total_writes);
        return err;
    }

    return PPDB_OK;
}

// Cleanup transaction management
void ppdb_engine_txn_cleanup(ppdb_engine_t* engine) {
    if (!engine) return;

    if (engine->txn_mutex) {
        ppdb_base_mutex_destroy(engine->txn_mutex);
    }

    ppdb_base_counter_destroy(engine->stats.total_txns);
    ppdb_base_counter_destroy(engine->stats.active_txns);
    ppdb_base_counter_destroy(engine->stats.total_reads);
    ppdb_base_counter_destroy(engine->stats.total_writes);
}

// Begin a new transaction
ppdb_error_t ppdb_engine_txn_begin(ppdb_engine_t* engine, ppdb_engine_txn_t** txn) {
    ppdb_engine_txn_t* new_txn;
    ppdb_error_t err;

    if (!engine || !txn) return PPDB_ENGINE_ERR_PARAM;

    new_txn = ppdb_base_aligned_alloc(sizeof(void*), sizeof(ppdb_engine_txn_t));
    if (!new_txn) return PPDB_ERR_MEMORY;

    // 初始化事务统计
    err = ppdb_base_counter_create(&new_txn->stats.reads);
    if (err != PPDB_OK) {
        ppdb_base_aligned_free(new_txn);
        return err;
    }

    err = ppdb_base_counter_create(&new_txn->stats.writes);
    if (err != PPDB_OK) {
        ppdb_base_counter_destroy(new_txn->stats.reads);
        ppdb_base_aligned_free(new_txn);
        return err;
    }

    // 更新事务计数
    ppdb_base_counter_increment(engine->stats.total_txns);
    ppdb_base_counter_increment(engine->stats.active_txns);

    // 设置事务状态
    new_txn->stats.is_active = true;
    new_txn->stats.is_committed = false;
    new_txn->stats.is_rolledback = false;

    *txn = new_txn;
    return PPDB_OK;
}

// Commit a transaction
ppdb_error_t ppdb_engine_txn_commit(ppdb_engine_txn_t* txn) {
    if (!txn || !txn->stats.is_active) return PPDB_ENGINE_ERR_PARAM;

    // 更新事务状态
    txn->stats.is_active = false;
    txn->stats.is_committed = true;

    // 更新引擎统计
    ppdb_base_counter_decrement(txn->engine->stats.active_txns);

    return PPDB_OK;
}

// Rollback a transaction
ppdb_error_t ppdb_engine_txn_rollback(ppdb_engine_txn_t* txn) {
    if (!txn || !txn->stats.is_active) return PPDB_ENGINE_ERR_PARAM;

    // 更新事务状态
    txn->stats.is_active = false;
    txn->stats.is_rolledback = true;

    // 更新引擎统计
    ppdb_base_counter_decrement(txn->engine->stats.active_txns);

    return PPDB_OK;
}

// Get transaction statistics
void ppdb_engine_txn_get_stats(ppdb_engine_txn_t* txn, ppdb_engine_txn_stats_t* stats) {
    if (!txn || !stats) return;

    stats->reads = txn->stats.reads;
    stats->writes = txn->stats.writes;
    stats->is_active = txn->stats.is_active;
    stats->is_committed = txn->stats.is_committed;
    stats->is_rolledback = txn->stats.is_rolledback;
}