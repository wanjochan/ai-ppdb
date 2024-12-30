#include <cosmopolitan.h>
#include "ppdb/ppdb_kvstore.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_fs.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "kvstore/internal/kvstore_wal.h"
#include "kvstore/internal/kvstore_wal_types.h"
#include "kvstore/internal/kvstore_fs.h"
#include "kvstore/internal/sync.h"
#include "kvstore/internal/metrics.h"

// CRC32计算函数
static uint32_t calculate_crc32(const void* data, size_t size) {
    if (!data || size == 0) {
        return 0;
    }
    return crc32c(0, data, size);
}

// 基础WAL操作实现
ppdb_error_t ppdb_wal_create_basic(const ppdb_wal_config_t* config, ppdb_wal_t** wal) {
    if (!config || !wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_wal_t* new_wal = (ppdb_wal_t*)calloc(1, sizeof(ppdb_wal_t));
    if (!new_wal) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 初始化WAL头部
    wal_header_t header = {
        .magic = WAL_MAGIC,
        .version = WAL_VERSION,
        .sequence = 0
    };

    // 复制配置
    new_wal->dir_path = strdup(config->dir_path);
    if (!new_wal->dir_path) {
        free(new_wal);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    new_wal->filename = strdup(config->filename);
    if (!new_wal->filename) {
        free(new_wal->dir_path);
        free(new_wal);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    new_wal->file_size = sizeof(header);
    new_wal->sync_on_write = config->sync_on_write;
    new_wal->enable_compression = config->enable_compression;

    // 初始化同步对象
    ppdb_sync_config_t sync_config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 1000
    };

    ppdb_error_t err = ppdb_sync_init(new_wal->sync, &sync_config);
    if (err != PPDB_OK) {
        free(new_wal->filename);
        free(new_wal->dir_path);
        free(new_wal);
        return err;
    }

    // 如果启用了缓冲区，则初始化缓冲区
    if (config->use_buffer) {
        new_wal->buffers = malloc(sizeof(wal_buffer_t));
        if (!new_wal->buffers) {
            ppdb_sync_destroy(new_wal->sync);
            free(new_wal->filename);
            free(new_wal->dir_path);
            free(new_wal);
            return PPDB_ERR_OUT_OF_MEMORY;
        }

        new_wal->buffers[0].data = malloc(config->buffer_size);
        if (!new_wal->buffers[0].data) {
            free(new_wal->buffers);
            ppdb_sync_destroy(new_wal->sync);
            free(new_wal->filename);
            free(new_wal->dir_path);
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
            ppdb_sync_destroy(new_wal->sync);
            free(new_wal->filename);
            free(new_wal->dir_path);
            free(new_wal);
            return err;
        }

        new_wal->buffer_count = 1;
    } else {
        new_wal->buffers = NULL;
        new_wal->buffer_count = 0;
    }

    // 初始化其他字段
    new_wal->closed = false;
    new_wal->current_buffer = 0;
    ppdb_metrics_init(&new_wal->metrics);

    *wal = new_wal;
    return PPDB_OK;
}

void ppdb_wal_destroy_basic(ppdb_wal_t* wal) {
    if (!wal) {
        return;
    }

    // 清理缓冲区
    if (wal->buffers) {
        for (size_t i = 0; i < wal->buffer_count; i++) {
            ppdb_sync_destroy(&wal->buffers[i].sync);
            free(wal->buffers[i].data);
        }
        free(wal->buffers);
    }

    // 清理其他资源
    free(wal->filename);
    free(wal->dir_path);
    ppdb_sync_destroy(wal->sync);
    free(wal);
}

ppdb_error_t ppdb_wal_write_basic(ppdb_wal_t* wal, const void* key, size_t key_len,
                                 const void* value, size_t value_len) {
    if (!wal || !key || key_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_sync_lock(wal->sync);

    // 准备记录头部
    wal_record_header_t header = {
        .type = PPDB_WAL_RECORD_PUT,
        .key_size = key_len,
        .value_size = value_len,
        .crc32 = 0  // 先设为0，后面计算
    };

    // 计算CRC32
    header.crc32 = calculate_crc32(key, key_len);
    if (value && value_len > 0) {
        header.crc32 ^= calculate_crc32(value, value_len);
    }

    // 写入记录头部
    ppdb_error_t err = ppdb_write_file(wal->filename, &header, sizeof(header));
    if (err != PPDB_OK) {
        ppdb_sync_unlock(wal->sync);
        return err;
    }

    // 写入键
    err = ppdb_write_file(wal->filename, key, key_len);
    if (err != PPDB_OK) {
        ppdb_sync_unlock(wal->sync);
        return err;
    }

    // 写入值（如果有）
    if (value && value_len > 0) {
        err = ppdb_write_file(wal->filename, value, value_len);
        if (err != PPDB_OK) {
            ppdb_sync_unlock(wal->sync);
            return err;
        }
    }

    // 更新文件大小
    wal->file_size += sizeof(header) + key_len + value_len;

    // 如果需要，同步到磁盘
    if (wal->sync_on_write) {
        err = ppdb_sync_file(wal->filename);
        if (err != PPDB_OK) {
            ppdb_sync_unlock(wal->sync);
            return err;
        }
    }

    // 更新指标
    wal->metrics.put_count++;

    ppdb_sync_unlock(wal->sync);
    return PPDB_OK;
}

ppdb_error_t ppdb_wal_write_lockfree_basic(ppdb_wal_t* wal, const void* key, size_t key_len,
                                          const void* value, size_t value_len) {
    if (!wal || !key || key_len == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 准备记录头部
    wal_record_header_t header = {
        .type = PPDB_WAL_RECORD_PUT,
        .key_size = key_len,
        .value_size = value_len,
        .crc32 = 0  // TODO: 计算CRC32
    };

    // 获取当前文件大小
    size_t current_size;
    ppdb_error_t err = ppdb_get_file_size(wal->filename, &current_size);
    if (err != PPDB_OK) {
        return err;
    }

    // 写入记录头部
    err = ppdb_append_file(wal->filename, &header, sizeof(header));
    if (err != PPDB_OK) {
        return err;
    }

    // 写入键
    err = ppdb_append_file(wal->filename, key, key_len);
    if (err != PPDB_OK) {
        return err;
    }

    // 写入值（如果有）
    err = ppdb_append_file(wal->filename, value, value_len);
    if (err != PPDB_OK) {
        return err;
    }

    // 更新文件大小
    wal->file_size = current_size + sizeof(header) + key_len + value_len;
    wal->metrics.put_count++;

    return PPDB_OK;
}

ppdb_error_t ppdb_wal_sync_basic(ppdb_wal_t* wal) {
    if (!wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_sync_lock(wal->sync);

    // 更新指标
    wal->metrics.total_ops++;

    ppdb_sync_unlock(wal->sync);
    return PPDB_OK;
}

ppdb_error_t ppdb_wal_sync_lockfree_basic(ppdb_wal_t* wal) {
    if (!wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 更新指标
    wal->metrics.total_ops++;

    return PPDB_OK;
}

size_t ppdb_wal_size_basic(ppdb_wal_t* wal) {
    if (!wal) {
        return 0;
    }

    size_t current_size;
    if (ppdb_get_file_size(wal->filename, &current_size) != PPDB_OK) {
        return 0;
    }

    return current_size;
}

size_t ppdb_wal_size_lockfree_basic(ppdb_wal_t* wal) {
    return wal ? wal->file_size : 0;
}

uint64_t ppdb_wal_next_sequence_basic(ppdb_wal_t* wal) {
    return wal ? wal->next_sequence : 0;
}

uint64_t ppdb_wal_next_sequence_lockfree_basic(ppdb_wal_t* wal) {
    return wal ? wal->next_sequence : 0;
}

ppdb_error_t ppdb_wal_recovery_iter_create_basic(ppdb_wal_t* wal, ppdb_wal_recovery_iter_t** iter) {
    if (!wal || !iter) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_wal_recovery_iter_t* new_iter = malloc(sizeof(ppdb_wal_recovery_iter_t));
    if (!new_iter) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    new_iter->wal = wal;
    new_iter->position = sizeof(wal_header_t);  // 跳过WAL头部
    new_iter->buffer = NULL;
    new_iter->buffer_size = 0;
    new_iter->current.key = NULL;
    new_iter->current.key_size = 0;
    new_iter->current.value = NULL;
    new_iter->current.value_size = 0;

    *iter = new_iter;
    return PPDB_OK;
}

void ppdb_wal_recovery_iter_destroy_basic(ppdb_wal_recovery_iter_t* iter) {
    if (!iter) {
        return;
    }

    free(iter->current.key);
    free(iter->current.value);
    free(iter->buffer);
    free(iter);
}

ppdb_error_t ppdb_wal_recovery_iter_next_basic(ppdb_wal_recovery_iter_t* iter,
                                              void** key, size_t* key_len,
                                              void** value, size_t* value_len) {
    if (!iter || !key || !key_len || !value || !value_len) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 检查是否已到文件末尾
    if (iter->position >= (off_t)iter->wal->file_size) {
        return PPDB_ERR_ITERATOR_END;
    }

    // 读取记录头部
    wal_record_header_t header;
    ssize_t bytes_read = pread(iter->wal->current_fd, &header, sizeof(header), iter->position);
    if (bytes_read != sizeof(header)) {
        return PPDB_ERR_IO;
    }

    // 验证记录大小
    if (header.key_size == 0 || header.key_size > PPDB_MAX_KEY_SIZE ||
        header.value_size > PPDB_MAX_VALUE_SIZE) {
        return PPDB_ERR_WAL_CORRUPTED;
    }

    // 分配内存
    void* key_data = malloc(header.key_size);
    void* value_data = header.value_size > 0 ? malloc(header.value_size) : NULL;

    if (!key_data || (header.value_size > 0 && !value_data)) {
        free(key_data);
        free(value_data);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 读取键
    off_t data_pos = iter->position + sizeof(header);
    bytes_read = pread(iter->wal->current_fd, key_data, header.key_size, data_pos);
    if (bytes_read != header.key_size) {
        free(key_data);
        free(value_data);
        return PPDB_ERR_IO;
    }

    // 读取值（如果有）
    if (header.value_size > 0) {
        data_pos += header.key_size;
        bytes_read = pread(iter->wal->current_fd, value_data, header.value_size, data_pos);
        if (bytes_read != header.value_size) {
            free(key_data);
            free(value_data);
            return PPDB_ERR_IO;
        }
    }

    // 更新迭代器位置
    iter->position += sizeof(header) + header.key_size + header.value_size;

    // 返回数据
    *key = key_data;
    *key_len = header.key_size;
    *value = value_data;
    *value_len = header.value_size;

    return PPDB_OK;
}

ppdb_error_t ppdb_wal_recover_basic(ppdb_wal_t* wal, ppdb_memtable_t* memtable) {
    if (!wal || !memtable) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 创建迭代器
    ppdb_wal_recovery_iter_t* iter;
    ppdb_error_t err = ppdb_wal_recovery_iter_create(wal, &iter);
    if (err != PPDB_OK) {
        return err;
    }

    // 遍历WAL记录
    void* key;
    size_t key_len;
    void* value;
    size_t value_len;

    while (true) {
        err = ppdb_wal_recovery_iter_next(iter,
                                        &key, &key_len,
                                        &value, &value_len);

        if (err == PPDB_ERR_ITERATOR_END) {
            break;
        }

        if (err != PPDB_OK) {
            ppdb_wal_recovery_iter_destroy(iter);
            return err;
        }

        // 将记录写入内存表
        err = ppdb_memtable_put(memtable, key, key_len, value, value_len);

        free(key);
        free(value);

        if (err != PPDB_OK) {
            ppdb_wal_recovery_iter_destroy(iter);
            return err;
        }
    }

    ppdb_wal_recovery_iter_destroy(iter);
    return PPDB_OK;
}

ppdb_error_t ppdb_wal_recover_lockfree_basic(ppdb_wal_t* wal, ppdb_memtable_t* memtable) {
    // 对于基础实现，我们直接调用带锁的版本
    return ppdb_wal_recover_basic(wal, memtable);
}
