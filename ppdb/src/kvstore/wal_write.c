#include <cosmopolitan.h>
#include "ppdb/ppdb_kvstore.h"
#include "ppdb/ppdb_error.h"
#include "kvstore/internal/kvstore_wal.h"
#include "kvstore/internal/kvstore_wal_types.h"
#include "ppdb/ppdb_logger.h"

// 写入单条记录
static ppdb_error_t write_record(ppdb_wal_t* wal, const void* key, size_t key_size,
                               const void* value, size_t value_size) {
    if (!wal || !key || !value) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 准备记录头部
    wal_record_header_t header = {
        .magic = WAL_MAGIC,
        .type = PPDB_WAL_RECORD_PUT,
        .key_size = key_size,
        .value_size = value_size,
        .sequence = wal->next_sequence++,
        .checksum = 0
    };

    // 计算校验和
    header.checksum = calculate_crc32(&header, sizeof(header));
    header.checksum = calculate_crc32(key, key_size);
    header.checksum = calculate_crc32(value, value_size);

    // 写入头部
    ssize_t write_size = write(wal->current_fd, &header, sizeof(header));
    if (write_size != sizeof(header)) {
        return PPDB_ERR_IO;
    }

    // 写入键
    write_size = write(wal->current_fd, key, key_size);
    if (write_size != key_size) {
        return PPDB_ERR_IO;
    }

    // 写入值
    write_size = write(wal->current_fd, value, value_size);
    if (write_size != value_size) {
        return PPDB_ERR_IO;
    }

    // 更新当前段大小
    wal->current_size += sizeof(header) + key_size + value_size;

    // 如果需要同步写入
    if (wal->sync_on_write) {
        if (fsync(wal->current_fd) != 0) {
            return PPDB_ERR_IO;
        }
    }

    return PPDB_OK;
}

// 批量写入
ppdb_error_t ppdb_wal_write_batch(ppdb_wal_t* wal, const ppdb_write_batch_t* batch) {
    if (!wal || !batch || !batch->ops || batch->count == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (wal->closed) {
        return PPDB_ERR_WAL_CLOSED;
    }

    ppdb_error_t err = PPDB_OK;
    ppdb_sync_lock(wal->sync);

    // 计算批量写入的总大小
    size_t total_size = 0;
    for (size_t i = 0; i < batch->count; i++) {
        total_size += sizeof(wal_record_header_t) + 
                     batch->ops[i].key_size + 
                     batch->ops[i].value_size;
    }

    // 检查是否需要切换到新段
    if (wal->current_size + total_size > wal->config.segment_size) {
        err = roll_new_segment(wal);
        if (err != PPDB_OK) {
            ppdb_sync_unlock(wal->sync);
            return err;
        }
    }

    // 写入所有记录
    for (size_t i = 0; i < batch->count; i++) {
        err = write_record(wal, batch->ops[i].key, batch->ops[i].key_size,
                          batch->ops[i].value, batch->ops[i].value_size);
        if (err != PPDB_OK) {
            ppdb_sync_unlock(wal->sync);
            return err;
        }
    }

    ppdb_sync_unlock(wal->sync);
    return PPDB_OK;
}

// 无锁批量写入
ppdb_error_t ppdb_wal_write_batch_lockfree(ppdb_wal_t* wal, const ppdb_write_batch_t* batch) {
    if (!wal || !batch || !batch->ops || batch->count == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (wal->closed) {
        return PPDB_ERR_WAL_CLOSED;
    }

    ppdb_error_t err = PPDB_OK;

    // 计算批量写入的总大小
    size_t total_size = 0;
    for (size_t i = 0; i < batch->count; i++) {
        total_size += sizeof(wal_record_header_t) + 
                     batch->ops[i].key_size + 
                     batch->ops[i].value_size;
    }

    // 检查是否需要切换到新段
    if (wal->current_size + total_size > wal->config.segment_size) {
        err = roll_new_segment(wal);
        if (err != PPDB_OK) {
            return err;
        }
    }

    // 写入所有记录
    for (size_t i = 0; i < batch->count; i++) {
        err = write_record(wal, batch->ops[i].key, batch->ops[i].key_size,
                          batch->ops[i].value, batch->ops[i].value_size);
        if (err != PPDB_OK) {
            return err;
        }
    }

    return PPDB_OK;
}

// 写入单条记录（带锁）
ppdb_error_t ppdb_wal_write(ppdb_wal_t* wal, const void* key, size_t key_size,
                           const void* value, size_t value_size) {
    if (!wal || !key || !value) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (wal->closed) {
        return PPDB_ERR_WAL_CLOSED;
    }

    ppdb_error_t err = PPDB_OK;
    ppdb_sync_lock(wal->sync);

    // 检查是否需要切换到新段
    size_t record_size = sizeof(wal_record_header_t) + key_size + value_size;
    if (wal->current_size + record_size > wal->config.segment_size) {
        err = roll_new_segment(wal);
        if (err != PPDB_OK) {
            ppdb_sync_unlock(wal->sync);
            return err;
        }
    }

    // 写入记录
    err = write_record(wal, key, key_size, value, value_size);

    ppdb_sync_unlock(wal->sync);
    return err;
}

// 写入单条记录（无锁）
ppdb_error_t ppdb_wal_write_lockfree(ppdb_wal_t* wal, const void* key, size_t key_size,
                                    const void* value, size_t value_size) {
    if (!wal || !key || !value) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (wal->closed) {
        return PPDB_ERR_WAL_CLOSED;
    }

    // 检查是否需要切换到新段
    size_t record_size = sizeof(wal_record_header_t) + key_size + value_size;
    if (wal->current_size + record_size > wal->config.segment_size) {
        ppdb_error_t err = roll_new_segment(wal);
        if (err != PPDB_OK) {
            return err;
        }
    }

    // 写入记录
    return write_record(wal, key, key_size, value, value_size);
} 