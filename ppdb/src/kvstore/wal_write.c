#include <cosmopolitan.h>
#include "ppdb/ppdb_kvstore.h"
#include "ppdb/ppdb_error.h"
#include "kvstore/internal/kvstore_wal.h"
#include "kvstore/internal/kvstore_wal_types.h"
#include "ppdb/ppdb_logger.h"

// 前向声明
static ppdb_error_t seal_segment(wal_segment_t* segment);
static ppdb_error_t create_new_segment(ppdb_wal_t* wal, wal_segment_t** segment);
static ppdb_error_t cleanup_old_segments(ppdb_wal_t* wal);

// 封存段
static ppdb_error_t seal_segment(wal_segment_t* segment) {
    if (!segment || segment->is_sealed) return PPDB_OK;
    
    // 更新段头部
    wal_segment_header_t header;
    if (pread(segment->fd, &header, WAL_SEGMENT_HEADER_SIZE, 0) != WAL_SEGMENT_HEADER_SIZE) {
        return PPDB_ERR_IO;
    }

    header.last_sequence = segment->last_sequence;
    header.checksum = 0;
    header.checksum = calculate_crc32(&header, WAL_SEGMENT_HEADER_SIZE);

    if (pwrite(segment->fd, &header, WAL_SEGMENT_HEADER_SIZE, 0) != WAL_SEGMENT_HEADER_SIZE) {
        return PPDB_ERR_IO;
    }

    // 同步文件
    if (fsync(segment->fd) != 0) {
        return PPDB_ERR_IO;
    }

    segment->is_sealed = true;
    return PPDB_OK;
}

// 创建新段
static ppdb_error_t create_new_segment(ppdb_wal_t* wal, wal_segment_t** segment) {
    wal_segment_t* new_segment = malloc(sizeof(wal_segment_t));
    if (!new_segment) return PPDB_ERR_OUT_OF_MEMORY;

    new_segment->id = wal->next_segment_id++;
    new_segment->filename = generate_segment_filename(wal->dir_path, new_segment->id);
    if (!new_segment->filename) {
        free(new_segment);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 创建并打开段文件
    new_segment->fd = open(new_segment->filename, O_CREAT | O_RDWR, 0644);
    if (new_segment->fd < 0) {
        free(new_segment->filename);
        free(new_segment);
        return PPDB_ERR_IO;
    }

    // 写入段头部
    wal_segment_header_t header = {
        .magic = WAL_MAGIC,
        .version = WAL_VERSION,
        .first_sequence = wal->next_sequence,
        .last_sequence = wal->next_sequence - 1,  // 还没有记录
        .record_count = 0,
        .checksum = 0
    };

    // 计算校验和
    header.checksum = calculate_crc32(&header, WAL_SEGMENT_HEADER_SIZE);

    if (write(new_segment->fd, &header, WAL_SEGMENT_HEADER_SIZE) != WAL_SEGMENT_HEADER_SIZE) {
        close(new_segment->fd);
        unlink(new_segment->filename);
        free(new_segment->filename);
        free(new_segment);
        return PPDB_ERR_IO;
    }

    new_segment->size = WAL_SEGMENT_HEADER_SIZE;
    new_segment->next = NULL;
    new_segment->is_sealed = false;
    new_segment->first_sequence = header.first_sequence;
    new_segment->last_sequence = header.last_sequence;

    // 将新段添加到链表末尾
    if (!wal->segments) {
        wal->segments = new_segment;
    } else {
        wal_segment_t* last = wal->segments;
        while (last->next) {
            last = last->next;
        }
        last->next = new_segment;
    }

    wal->segment_count++;
    *segment = new_segment;
    return PPDB_OK;
}

// 清理旧段
static ppdb_error_t cleanup_old_segments(ppdb_wal_t* wal) {
    if (!wal || wal->segment_count <= wal->config.max_segments) {
        return PPDB_OK;
    }

    // 保留最新的段
    size_t segments_to_remove = wal->segment_count - wal->config.max_segments;
    wal_segment_t* current = wal->segments;
    wal_segment_t* prev = NULL;

    for (size_t i = 0; i < segments_to_remove && current; i++) {
        wal_segment_t* to_remove = current;
        current = current->next;

        // 关闭并删除文件
        close(to_remove->fd);
        unlink(to_remove->filename);
        free(to_remove->filename);
        free(to_remove);

        wal->segment_count--;
        wal->total_size -= to_remove->size;
    }

    // 更新段链表头
    wal->segments = current;
    return PPDB_OK;
}

// 切换到新段
ppdb_error_t roll_new_segment(ppdb_wal_t* wal) {
    if (!wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 封存当前段
    if (wal->segments) {
        wal_segment_t* curr = wal->segments;
        while (curr->next) {
            curr = curr->next;
        }
        ppdb_error_t err = seal_segment(curr);
        if (err != PPDB_OK) {
            return err;
        }
    }

    // 创建新段
    wal_segment_t* new_segment;
    ppdb_error_t err = create_new_segment(wal, &new_segment);
    if (err != PPDB_OK) {
        return err;
    }

    // 更新当前段信息
    wal->current_fd = new_segment->fd;
    wal->current_size = new_segment->size;

    // 清理旧段
    err = cleanup_old_segments(wal);
    if (err != PPDB_OK) {
        return err;
    }

    return PPDB_OK;
}

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
    wal->total_size += sizeof(header) + key_size + value_size;

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