#include <cosmopolitan.h>
#include "ppdb/ppdb_kvstore.h"
#include "ppdb/ppdb_error.h"
#include "kvstore/internal/kvstore_wal.h"
#include "kvstore/internal/kvstore_wal_types.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "common/logger.h"

// 恢复 WAL 数据到内存表
ppdb_error_t ppdb_wal_recover(ppdb_wal_t* wal, ppdb_memtable_t* memtable) {
    if (!wal || !memtable) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err = PPDB_OK;
    ppdb_wal_iterator_t* iterator = NULL;

    // 创建迭代器
    err = ppdb_wal_iterator_create(wal, &iterator);
    if (err != PPDB_OK) {
        return err;
    }

    // 遍历所有记录
    while (ppdb_wal_iterator_valid(iterator)) {
        void* key = NULL;
        void* value = NULL;
        size_t key_size = 0;
        size_t value_size = 0;

        // 获取当前记录
        err = ppdb_wal_iterator_get(iterator, &key, &key_size, &value, &value_size);
        if (err != PPDB_OK) {
            ppdb_wal_iterator_destroy(iterator);
            return err;
        }

        // 写入内存表
        err = ppdb_memtable_put(memtable, key, key_size, value, value_size);
        free(key);
        free(value);
        if (err != PPDB_OK) {
            ppdb_wal_iterator_destroy(iterator);
            return err;
        }

        // 移动到下一条记录
        err = ppdb_wal_iterator_next(iterator);
        if (err != PPDB_OK) {
            ppdb_wal_iterator_destroy(iterator);
            return err;
        }
    }

    ppdb_wal_iterator_destroy(iterator);
    return PPDB_OK;
}

// 从指定序列号开始恢复
ppdb_error_t ppdb_wal_recover_from(ppdb_wal_t* wal, ppdb_memtable_t* memtable, 
                                  uint64_t start_sequence) {
    if (!wal || !memtable) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err = PPDB_OK;
    ppdb_wal_iterator_t* iterator = NULL;

    // 创建迭代器
    err = ppdb_wal_iterator_create(wal, &iterator);
    if (err != PPDB_OK) {
        return err;
    }

    // 跳转到指定序列号
    err = ppdb_wal_iterator_seek(iterator, start_sequence);
    if (err != PPDB_OK) {
        ppdb_wal_iterator_destroy(iterator);
        return err;
    }

    // 遍历所有记录
    while (ppdb_wal_iterator_valid(iterator)) {
        void* key = NULL;
        void* value = NULL;
        size_t key_size = 0;
        size_t value_size = 0;

        // 获取当前记录
        err = ppdb_wal_iterator_get(iterator, &key, &key_size, &value, &value_size);
        if (err != PPDB_OK) {
            ppdb_wal_iterator_destroy(iterator);
            return err;
        }

        // 写入内存表
        err = ppdb_memtable_put(memtable, key, key_size, value, value_size);
        free(key);
        free(value);
        if (err != PPDB_OK) {
            ppdb_wal_iterator_destroy(iterator);
            return err;
        }

        // 移动到下一条记录
        err = ppdb_wal_iterator_next(iterator);
        if (err != PPDB_OK) {
            ppdb_wal_iterator_destroy(iterator);
            return err;
        }
    }

    ppdb_wal_iterator_destroy(iterator);
    return PPDB_OK;
}

// 验证 WAL 数据一致性
ppdb_error_t ppdb_wal_verify(ppdb_wal_t* wal) {
    if (!wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err = PPDB_OK;
    ppdb_wal_iterator_t* iterator = NULL;

    // 创建迭代器
    err = ppdb_wal_iterator_create(wal, &iterator);
    if (err != PPDB_OK) {
        return err;
    }

    uint64_t prev_sequence = 0;
    bool first_record = true;

    // 遍历所有记录验证序列号连续性
    while (ppdb_wal_iterator_valid(iterator)) {
        void* key = NULL;
        void* value = NULL;
        size_t key_size = 0;
        size_t value_size = 0;

        // 获取当前记录
        err = ppdb_wal_iterator_get(iterator, &key, &key_size, &value, &value_size);
        if (err != PPDB_OK) {
            ppdb_wal_iterator_destroy(iterator);
            return err;
        }

        // 验证序列号
        uint64_t curr_sequence = ppdb_wal_iterator_sequence(iterator);
        if (!first_record && curr_sequence != prev_sequence + 1) {
            free(key);
            free(value);
            ppdb_wal_iterator_destroy(iterator);
            return PPDB_ERR_CORRUPTED;
        }

        prev_sequence = curr_sequence;
        first_record = false;

        free(key);
        free(value);

        // 移动到下一条记录
        err = ppdb_wal_iterator_next(iterator);
        if (err != PPDB_OK) {
            ppdb_wal_iterator_destroy(iterator);
            return err;
        }
    }

    ppdb_wal_iterator_destroy(iterator);
    return PPDB_OK;
}

// 获取恢复点信息
ppdb_error_t ppdb_wal_get_recovery_point(ppdb_wal_t* wal, 
                                        ppdb_wal_recovery_point_t* recovery_point) {
    if (!wal || !recovery_point) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 获取最小和最大序列号
    uint64_t min_sequence = UINT64_MAX;
    uint64_t max_sequence = 0;

    wal_segment_t* curr = wal->segments;
    while (curr) {
        if (curr->first_sequence < min_sequence) {
            min_sequence = curr->first_sequence;
        }
        if (curr->last_sequence > max_sequence) {
            max_sequence = curr->last_sequence;
        }
        curr = curr->next;
    }

    recovery_point->min_sequence = min_sequence;
    recovery_point->max_sequence = max_sequence;
    recovery_point->total_segments = wal->segment_count;

    return PPDB_OK;
} 