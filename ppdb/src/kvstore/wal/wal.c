#include "ppdb/wal/wal.h"
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

// WAL记录头
typedef struct wal_record_header {
    uint32_t type;           // 记录类型
    uint32_t key_size;       // 键大小
    uint32_t value_size;     // 值大小
    uint64_t sequence;       // 序列号
    uint32_t checksum;       // 校验和
} wal_record_header_t;

// WAL缓冲区
typedef struct wal_buffer {
    char* data;              // 数据
    size_t size;            // 大小
    size_t used;            // 已使用
    bool in_use;            // 是否使用中
} wal_buffer_t;

// WAL组提交
typedef struct wal_group {
    struct {
        void* data;          // 数据
        size_t size;         // 大小
    } records[16];           // 记录数组
    size_t count;            // 记录数量
    bool committing;         // 是否提交中
} wal_group_t;

// WAL结构
struct ppdb_wal {
    int fd;                  // 文件描述符
    ppdb_sync_t sync;       // 同步机制
    wal_buffer_t* buffers;  // 缓冲区数组
    size_t buffer_count;    // 缓冲区数量
    size_t current_buffer;  // 当前缓冲区
    wal_group_t group;      // 组提交
    bool enable_group_commit;// 启用组提交
    uint32_t group_interval; // 组提交间隔
    bool enable_async_flush; // 启用异步刷盘
    bool enable_checksum;    // 启用校验和
    uint64_t file_size;     // 文件大小
};

// WAL恢复迭代器
struct ppdb_wal_recovery_iter {
    ppdb_wal_t* wal;        // WAL指针
    off_t position;         // 当前位置
    char* buffer;           // 读取缓冲区
    size_t buffer_size;     // 缓冲区大小
};

// 计算校验和
static uint32_t calculate_checksum(const void* data, size_t len) {
    // TODO: 实现CRC32
    return 0;
}

// 创建WAL
ppdb_wal_t* ppdb_wal_create(const char* filename, const ppdb_wal_config_t* config) {
    if (!filename || !config) return NULL;

    ppdb_wal_t* wal = (ppdb_wal_t*)malloc(sizeof(ppdb_wal_t));
    if (!wal) return NULL;

    // 初始化同步机制
    if (ppdb_sync_init(&wal->sync, &config->sync_config) != 0) {
        free(wal);
        return NULL;
    }

    // 打开文件
    wal->fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (wal->fd < 0) {
        ppdb_sync_destroy(&wal->sync);
        free(wal);
        return NULL;
    }

    // 初始化缓冲区
    wal->buffer_count = 2;  // 双缓冲
    wal->buffers = malloc(sizeof(wal_buffer_t) * wal->buffer_count);
    if (!wal->buffers) {
        close(wal->fd);
        ppdb_sync_destroy(&wal->sync);
        free(wal);
        return NULL;
    }

    for (size_t i = 0; i < wal->buffer_count; i++) {
        wal->buffers[i].data = malloc(config->buffer_size);
        if (!wal->buffers[i].data) {
            for (size_t j = 0; j < i; j++) {
                free(wal->buffers[j].data);
            }
            free(wal->buffers);
            close(wal->fd);
            ppdb_sync_destroy(&wal->sync);
            free(wal);
            return NULL;
        }
        wal->buffers[i].size = config->buffer_size;
        wal->buffers[i].used = 0;
        wal->buffers[i].in_use = false;
    }

    wal->current_buffer = 0;
    memset(&wal->group, 0, sizeof(wal->group));
    wal->enable_group_commit = config->enable_group_commit;
    wal->group_interval = config->group_commit_interval;
    wal->enable_async_flush = config->enable_async_flush;
    wal->enable_checksum = config->enable_checksum;
    wal->file_size = lseek(wal->fd, 0, SEEK_END);

    return wal;
}

// 销毁WAL
void ppdb_wal_destroy(ppdb_wal_t* wal) {
    if (!wal) return;

    // 刷新所有数据
    ppdb_wal_sync(wal);

    // 释放资源
    for (size_t i = 0; i < wal->buffer_count; i++) {
        free(wal->buffers[i].data);
    }
    free(wal->buffers);
    close(wal->fd);
    ppdb_sync_destroy(&wal->sync);
    free(wal);
}

// 追加记录
int ppdb_wal_append(ppdb_wal_t* wal, ppdb_wal_record_type_t type,
                    const void* key, size_t key_len,
                    const void* value, size_t value_len,
                    uint64_t sequence) {
    if (!wal || !key || (type == WAL_RECORD_PUT && !value)) return PPDB_ERROR;

    ppdb_sync_lock(&wal->sync);

    // 准备记录头
    wal_record_header_t header = {
        .type = type,
        .key_size = key_len,
        .value_size = value_len,
        .sequence = sequence,
        .checksum = 0
    };

    // 计算总大小
    size_t total_size = sizeof(header) + key_len + value_len;

    // 检查缓冲区空间
    wal_buffer_t* buffer = &wal->buffers[wal->current_buffer];
    if (buffer->used + total_size > buffer->size) {
        // 切换缓冲区
        wal->current_buffer = (wal->current_buffer + 1) % wal->buffer_count;
        buffer = &wal->buffers[wal->current_buffer];
        if (buffer->in_use) {
            // 等待缓冲区可用
            ppdb_wal_sync(wal);
        }
        buffer->used = 0;
    }

    // 写入记录头
    memcpy(buffer->data + buffer->used, &header, sizeof(header));
    buffer->used += sizeof(header);

    // 写入key
    memcpy(buffer->data + buffer->used, key, key_len);
    buffer->used += key_len;

    // 写入value
    if (type == WAL_RECORD_PUT) {
        memcpy(buffer->data + buffer->used, value, value_len);
        buffer->used += value_len;
    }

    // 计算校验和
    if (wal->enable_checksum) {
        header.checksum = calculate_checksum(buffer->data + buffer->used - total_size,
                                          total_size - sizeof(header.checksum));
        memcpy(buffer->data + buffer->used - total_size, &header, sizeof(header));
    }

    // 组提交
    if (wal->enable_group_commit) {
        wal->group.records[wal->group.count].data = buffer->data + buffer->used - total_size;
        wal->group.records[wal->group.count].size = total_size;
        wal->group.count++;

        if (wal->group.count >= 16) {
            ppdb_wal_sync(wal);
        }
    }

    ppdb_sync_unlock(&wal->sync);
    return PPDB_OK;
}

// 同步到磁盘
int ppdb_wal_sync(ppdb_wal_t* wal) {
    if (!wal) return PPDB_ERROR;

    ppdb_sync_lock(&wal->sync);

    // 写入所有缓冲区
    for (size_t i = 0; i < wal->buffer_count; i++) {
        wal_buffer_t* buffer = &wal->buffers[i];
        if (buffer->used > 0) {
            ssize_t written = write(wal->fd, buffer->data, buffer->used);
            if (written != buffer->used) {
                ppdb_sync_unlock(&wal->sync);
                return PPDB_ERROR;
            }
            buffer->used = 0;
            buffer->in_use = false;
        }
    }

    // 刷新到磁盘
    if (!wal->enable_async_flush) {
        fsync(wal->fd);
    }

    // 重置组提交
    wal->group.count = 0;
    wal->group.committing = false;

    ppdb_sync_unlock(&wal->sync);
    return PPDB_OK;
}

// 创建恢复迭代器
ppdb_wal_recovery_iter_t* ppdb_wal_recovery_iter_create(ppdb_wal_t* wal) {
    if (!wal) return NULL;

    ppdb_wal_recovery_iter_t* iter = malloc(sizeof(ppdb_wal_recovery_iter_t));
    if (!iter) return NULL;

    iter->wal = wal;
    iter->position = 0;
    iter->buffer_size = 4096;
    iter->buffer = malloc(iter->buffer_size);
    if (!iter->buffer) {
        free(iter);
        return NULL;
    }

    return iter;
}

// 销毁恢复迭代器
void ppdb_wal_recovery_iter_destroy(ppdb_wal_recovery_iter_t* iter) {
    if (!iter) return;
    free(iter->buffer);
    free(iter);
}

// 恢复迭代器是否有效
bool ppdb_wal_recovery_iter_valid(ppdb_wal_recovery_iter_t* iter) {
    if (!iter || !iter->wal) return false;
    return iter->position < iter->wal->file_size;
}

// 获取下一条记录
int ppdb_wal_recovery_iter_next(ppdb_wal_recovery_iter_t* iter,
                               ppdb_wal_record_type_t* type,
                               void** key, size_t* key_len,
                               void** value, size_t* value_len,
                               uint64_t* sequence) {
    if (!iter || !iter->wal || !type || !key || !key_len || !value || !value_len || !sequence) {
        return PPDB_ERROR;
    }

    // 读取记录头
    wal_record_header_t header;
    ssize_t read_size = pread(iter->wal->fd, &header, sizeof(header), iter->position);
    if (read_size != sizeof(header)) {
        return PPDB_ERROR;
    }

    // 校验记录
    if (iter->wal->enable_checksum) {
        // TODO: 验证校验和
    }

    // 读取key
    *key = malloc(header.key_size);
    if (!*key) return PPDB_ERROR;
    read_size = pread(iter->wal->fd, *key, header.key_size, 
                     iter->position + sizeof(header));
    if (read_size != header.key_size) {
        free(*key);
        return PPDB_ERROR;
    }

    // 读取value
    if (header.type == WAL_RECORD_PUT) {
        *value = malloc(header.value_size);
        if (!*value) {
            free(*key);
            return PPDB_ERROR;
        }
        read_size = pread(iter->wal->fd, *value, header.value_size,
                         iter->position + sizeof(header) + header.key_size);
        if (read_size != header.value_size) {
            free(*key);
            free(*value);
            return PPDB_ERROR;
        }
    } else {
        *value = NULL;
    }

    // 更新返回值
    *type = header.type;
    *key_len = header.key_size;
    *value_len = header.value_size;
    *sequence = header.sequence;

    // 移动位置
    iter->position += sizeof(header) + header.key_size + header.value_size;

    return PPDB_OK;
}
