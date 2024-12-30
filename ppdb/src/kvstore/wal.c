#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_types.h"
#include "ppdb/ppdb_kvstore.h"
#include "internal/kvstore_types.h"
#include "internal/kvstore_memtable.h"
#include "internal/kvstore_wal.h"
#include "internal/sync.h"
#include "internal/metrics.h"
#include "internal/kvstore_fs.h"

// CRC32计算函数
static uint32_t calculate_crc32(const void* data, size_t size) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* bytes = (const uint8_t*)data;
    
    for (size_t i = 0; i < size; i++) {
        crc ^= bytes[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    
    return ~crc;
}

// 实现函数
ppdb_error_t ppdb_wal_create_basic(const ppdb_wal_config_t* config, ppdb_wal_t** wal) {
    if (!config || !wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_wal_t* new_wal = aligned_alloc(64, sizeof(ppdb_wal_t));
    if (!new_wal) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 初始化文件头
    wal_header_t header = {
        .magic = WAL_MAGIC,
        .version = 1,
        .sequence = 0
    };

    // 写入文件头
    ppdb_error_t err = ppdb_write_file(config->filename, &header, sizeof(header));
    if (err != PPDB_OK) {
        free(new_wal);
        return err;
    }

    // 初始化 WAL 结构
    new_wal->filename = strdup(config->filename);
    if (!new_wal->filename) {
        free(new_wal);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    new_wal->file_size = sizeof(header);
    new_wal->sync_on_write = config->sync_on_write;
    new_wal->enable_compression = config->enable_compression;

    // 初始化同步原语
    ppdb_sync_config_t sync_config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 1000
    };

    err = ppdb_sync_init(&new_wal->sync, &sync_config);
    if (err != PPDB_OK) {
        free(new_wal->filename);
        free(new_wal);
        return err;
    }

    // 初始化缓冲区
    if (config->buffer_size > 0) {
        new_wal->buffers = malloc(sizeof(wal_buffer_t));
        if (!new_wal->buffers) {
            ppdb_sync_destroy(&new_wal->sync);
            free(new_wal->filename);
            free(new_wal);
            return PPDB_ERR_OUT_OF_MEMORY;
        }

        new_wal->buffers[0].data = malloc(config->buffer_size);
        if (!new_wal->buffers[0].data) {
            free(new_wal->buffers);
            ppdb_sync_destroy(&new_wal->sync);
            free(new_wal->filename);
            free(new_wal);
            return PPDB_ERR_OUT_OF_MEMORY;
        }

        new_wal->buffers[0].size = config->buffer_size;
        new_wal->buffers[0].used = 0;
        new_wal->buffers[0].in_use = false;
        err = ppdb_sync_init(&new_wal->buffers[0].sync, &sync_config);
        if (err != PPDB_OK) {
            free(new_wal->buffers[0].data);
            free(new_wal->buffers);
            ppdb_sync_destroy(&new_wal->sync);
            free(new_wal->filename);
            free(new_wal);
            return err;
        }

        new_wal->buffer_count = 1;
    } else {
        new_wal->buffers = NULL;
        new_wal->buffer_count = 0;
    }

    new_wal->next_sequence = 0;
    new_wal->closed = false;
    new_wal->current_buffer = 0;
    ppdb_metrics_init(&new_wal->metrics);

    *wal = new_wal;
    return PPDB_OK;
}

// Destroy WAL
void ppdb_wal_destroy_basic(ppdb_wal_t* wal) {
    if (!wal) return;
    
    // 清理缓冲区
    if (wal->buffers) {
        for (size_t i = 0; i < wal->buffer_count; i++) {
            ppdb_sync_destroy(&wal->buffers[i].sync);
            free(wal->buffers[i].data);
        }
        free(wal->buffers);
    }

    free(wal->filename);
    ppdb_sync_destroy(&wal->sync);
    free(wal);
}

// Write record
ppdb_error_t ppdb_wal_write_basic(ppdb_wal_t* wal, ppdb_wal_record_type_t type,
                                 const void* key, size_t key_size,
                                 const void* value, size_t value_size) {
    if (!wal || !key || key_size == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_sync_lock(&wal->sync);

    // 检查是否已关闭
    if (wal->closed) {
        ppdb_sync_unlock(&wal->sync);
        return PPDB_ERR_CLOSED;
    }

    // 准备记录头
    wal_record_header_t header = {
        .type = type,
        .key_size = key_size,
        .value_size = value_size,
        .crc32 = 0
    };

    // 计算CRC32
    header.crc32 = calculate_crc32(key, key_size);
    if (value && value_size > 0) {
        header.crc32 ^= calculate_crc32(value, value_size);
    }

    // 写入记录头
    ppdb_error_t err = ppdb_write_file(wal->filename, &header, sizeof(header));
    if (err != PPDB_OK) {
        ppdb_sync_unlock(&wal->sync);
        return err;
    }

    // 写入键
    err = ppdb_write_file(wal->filename, key, key_size);
    if (err != PPDB_OK) {
        ppdb_sync_unlock(&wal->sync);
        return err;
    }

    // 写入值
    if (value && value_size > 0) {
        err = ppdb_write_file(wal->filename, value, value_size);
        if (err != PPDB_OK) {
            ppdb_sync_unlock(&wal->sync);
            return err;
        }
    }

    // 更新文件大小
    wal->file_size += sizeof(header) + key_size + value_size;

    // 如果需要同步写入
    if (wal->sync_on_write) {
        err = ppdb_sync_file(wal->filename);
        if (err != PPDB_OK) {
            ppdb_sync_unlock(&wal->sync);
            return err;
        }
    }

    // 更新序列号和指标
    wal->next_sequence++;
    wal->metrics.put_count++;

    ppdb_sync_unlock(&wal->sync);
    return PPDB_OK;
}

// Write record (lockfree version)
ppdb_error_t ppdb_wal_write_lockfree_basic(ppdb_wal_t* wal, ppdb_wal_record_type_t type,
                                          const void* key, size_t key_len,
                                          const void* value, size_t value_len) {
    if (!wal || !key || !value) return PPDB_ERR_INVALID_ARG;

    // 写入记录头
    wal_record_header_t header = {
        .type = type,
        .key_size = key_len,
        .value_size = value_len,
        .crc32 = 0  // TODO: 计算CRC32
    };

    // 获取当前文件大小
    size_t current_size;
    ppdb_error_t err = ppdb_get_file_size(wal->filename, &current_size);
    if (err != PPDB_OK) return err;

    // 写入记录头
    err = ppdb_append_file(wal->filename, &header, sizeof(header));
    if (err != PPDB_OK) return err;

    // 写入键
    err = ppdb_append_file(wal->filename, key, key_len);
    if (err != PPDB_OK) return err;

    // 写入值
    err = ppdb_append_file(wal->filename, value, value_len);
    if (err != PPDB_OK) return err;

    // 更新状态
    wal->next_sequence++;
    wal->file_size = current_size + sizeof(header) + key_len + value_len;
    wal->metrics.put_count++;

    return PPDB_OK;
}

// Sync to disk
ppdb_error_t ppdb_wal_sync_basic(ppdb_wal_t* wal) {
    if (!wal) return PPDB_ERR_INVALID_ARG;

    ppdb_sync_lock(&wal->sync);

    // 在文件系统层面，我们目前没有显式的同步操作
    // 每次写入都是同步的，所以这里只需要更新计数器
    wal->metrics.total_ops++;

    ppdb_sync_unlock(&wal->sync);
    return PPDB_OK;
}

// Sync to disk (lockfree version)
ppdb_error_t ppdb_wal_sync_lockfree_basic(ppdb_wal_t* wal) {
    if (!wal) return PPDB_ERR_INVALID_ARG;

    // 同上，目前没有显式的同步操作
    wal->metrics.total_ops++;
    return PPDB_OK;
}

// Get file size
size_t ppdb_wal_size_basic(ppdb_wal_t* wal) {
    if (!wal) return 0;
    
    size_t current_size;
    if (ppdb_get_file_size(wal->filename, &current_size) != PPDB_OK) {
        return 0;
    }
    return current_size;
}

// Get file size (lockfree version)
size_t ppdb_wal_size_lockfree_basic(ppdb_wal_t* wal) {
    return ppdb_wal_size_basic(wal);  // 复用基础版本
}

// Get next sequence number
uint64_t ppdb_wal_next_sequence_basic(ppdb_wal_t* wal) {
    return wal ? wal->next_sequence : 0;
}

// Get next sequence number (lockfree version)
uint64_t ppdb_wal_next_sequence_lockfree_basic(ppdb_wal_t* wal) {
    return wal ? wal->next_sequence : 0;
}

// Create recovery iterator
ppdb_error_t ppdb_wal_recovery_iter_create_basic(ppdb_wal_t* wal,
                                                ppdb_wal_recovery_iter_t** iter) {
    if (!wal || !iter) return PPDB_ERR_INVALID_ARG;

    ppdb_wal_recovery_iter_t* new_iter = malloc(sizeof(ppdb_wal_recovery_iter_t));
    if (!new_iter) return PPDB_ERR_OUT_OF_MEMORY;

    new_iter->wal = wal;
    new_iter->position = sizeof(wal_header_t);  // Skip header
    new_iter->buffer = NULL;
    new_iter->buffer_size = 0;

    *iter = new_iter;
    return PPDB_OK;
}

// Destroy recovery iterator
void ppdb_wal_recovery_iter_destroy_basic(ppdb_wal_recovery_iter_t* iter) {
    if (!iter) return;
    free(iter->buffer);
    free(iter);
}

// Read next record with CRC check
ppdb_error_t ppdb_wal_recovery_iter_next_basic(ppdb_wal_recovery_iter_t* iter) {
    if (!iter || !iter->wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (iter->position >= iter->wal->file_size) {
        return PPDB_ERR_ITERATOR_END;
    }

    // 读取记录头
    wal_record_header_t header;
    size_t bytes_read;
    ppdb_error_t err = ppdb_read_file(iter->wal->filename, &header, sizeof(header), &bytes_read);
    if (err != PPDB_OK) {
        return err;
    }

    // 检查记录大小
    size_t total_size = header.key_size + header.value_size;
    if (total_size > iter->buffer_size) {
        return PPDB_ERR_BUFFER_TOO_SMALL;
    }

    // 读取记录数据
    err = ppdb_read_file(iter->wal->filename, iter->buffer, total_size, &bytes_read);
    if (err != PPDB_OK) {
        return err;
    }

    // 验证校验和
    uint32_t crc = calculate_crc32(iter->buffer, total_size);
    if (crc != header.crc32) {
        return PPDB_ERR_CHECKSUM;
    }

    // 设置当前记录
    iter->current.key = iter->buffer;
    iter->current.key_size = header.key_size;
    if (header.value_size > 0) {
        iter->current.value = iter->buffer + header.key_size;
        iter->current.value_size = header.value_size;
    } else {
        iter->current.value = NULL;
        iter->current.value_size = 0;
    }

    iter->position += sizeof(header) + total_size;
    return PPDB_OK;
}

// Recover WAL
ppdb_error_t ppdb_wal_recover_basic(ppdb_wal_t* wal, ppdb_memtable_t* memtable) {
    if (!wal || !memtable) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 创建恢复迭代器
    ppdb_wal_recovery_iter_t* iter;
    ppdb_error_t err = ppdb_wal_recovery_iter_create(wal, &iter);
    if (err != PPDB_OK) {
        return err;
    }

    // 遍历WAL记录
    void* key;
    size_t key_size;
    void* value;
    size_t value_size;

    while (err == PPDB_OK) {
        err = ppdb_wal_recovery_iter_next(iter,
                                        &key, &key_size,
                                        &value, &value_size);
        if (err == PPDB_ERR_ITERATOR_END) {
            err = PPDB_OK;
            break;
        }
        if (err != PPDB_OK) {
            break;
        }

        // 写入内存表
        err = ppdb_memtable_put_basic(memtable,
                                    key, key_size,
                                    value, value_size);
        if (err != PPDB_OK) {
            break;
        }
    }

    ppdb_wal_recovery_iter_destroy(iter);
    return err;
}

// Recover WAL (lockfree version)
ppdb_error_t ppdb_wal_recover_lockfree_basic(ppdb_wal_t* wal, ppdb_memtable_t* memtable) {
    if (!wal || !memtable) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 创建恢复迭代器
    ppdb_wal_recovery_iter_t* iter;
    ppdb_error_t err = ppdb_wal_recovery_iter_create(wal, &iter);
    if (err != PPDB_OK) {
        return err;
    }

    // 遍历WAL记录
    void* key;
    size_t key_size;
    void* value;
    size_t value_size;

    while (err == PPDB_OK) {
        err = ppdb_wal_recovery_iter_next(iter,
                                        &key, &key_size,
                                        &value, &value_size);
        if (err == PPDB_ERR_ITERATOR_END) {
            err = PPDB_OK;
            break;
        }
        if (err != PPDB_OK) {
            break;
        }

        // 写入内存表
        err = ppdb_memtable_put_lockfree_basic(memtable,
                                              key, key_size,
                                              value, value_size);
        if (err != PPDB_OK) {
            break;
        }
    }

    ppdb_wal_recovery_iter_destroy(iter);
    return err;
}
