#include <cosmopolitan.h>
#include <fcntl.h>
#include <unistd.h>

// 公共头文件
#include "ppdb/ppdb_error.h"

// 内部头文件
#include "internal/kvstore_types.h"
#include "internal/kvstore_wal.h"
#include "internal/kvstore_fs.h"
#include "internal/kvstore_logger.h"
#include "internal/sync.h"
#include "internal/metrics.h"

// WAL record header
typedef struct wal_record_header {
    uint32_t type;           // 记录类型
    uint32_t key_size;       // 键大小
    uint32_t value_size;     // 值大小
    uint64_t sequence;       // 序列号
    uint32_t checksum;       // 校验和
} __attribute__((packed)) wal_record_header_t;

// WAL buffer
typedef struct wal_buffer {
    char* data;              // 数据
    atomic_size_t size;      // 大小
    atomic_size_t used;      // 已用大小
    atomic_bool in_use;      // 是否使用中
    ppdb_sync_t sync;        // 同步原语
} wal_buffer_t;

// WAL structure
struct ppdb_wal {
    int fd;                  // 文件描述符
    char* filename;          // 文件名
    ppdb_wal_config_t config;// 配置
    ppdb_sync_t sync;        // 同步原语
    wal_buffer_t* buffers;   // 缓冲区数组
    atomic_size_t buffer_count;    // 缓冲区数量
    atomic_size_t current_buffer;  // 当前缓冲区
    atomic_uint64_t next_sequence; // 下一个序列号
    atomic_bool closed;            // 是否关闭
    atomic_uint64_t file_size;     // 文件大小
};

// WAL recovery iterator
struct ppdb_wal_recovery_iter {
    ppdb_wal_t* wal;        // WAL pointer
    off_t position;         // Current position
    char* buffer;           // Read buffer
    size_t buffer_size;     // Buffer size
};

// Create WAL
ppdb_wal_t* ppdb_wal_create(const char* filename, const ppdb_wal_config_t* config) {
    if (!filename || !config) return NULL;

    ppdb_wal_t* wal = aligned_alloc(64, sizeof(ppdb_wal_t));
    if (!wal) return NULL;

    // Initialize synchronization
    ppdb_sync_init(&wal->sync, &config->sync_config);

    // Open file
    wal->fd = open(filename, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (wal->fd < 0) {
        ppdb_sync_destroy(&wal->sync);
        free(wal);
        return NULL;
    }

    // Copy configuration
    memcpy(&wal->config, config, sizeof(ppdb_wal_config_t));
    wal->filename = strdup(filename);

    // Initialize buffers
    atomic_init(&wal->buffer_count, 2);  // Double buffer
    wal->buffers = aligned_alloc(64, sizeof(wal_buffer_t) * 2);
    if (!wal->buffers) {
        close(wal->fd);
        ppdb_sync_destroy(&wal->sync);
        free(wal->filename);
        free(wal);
        return NULL;
    }

    for (size_t i = 0; i < 2; i++) {
        wal->buffers[i].data = aligned_alloc(4096, config->buffer_size);
        if (!wal->buffers[i].data) {
            for (size_t j = 0; j < i; j++) {
                free(wal->buffers[j].data);
            }
            free(wal->buffers);
            close(wal->fd);
            ppdb_sync_destroy(&wal->sync);
            free(wal->filename);
            free(wal);
            return NULL;
        }
        atomic_init(&wal->buffers[i].size, config->buffer_size);
        atomic_init(&wal->buffers[i].used, 0);
        atomic_init(&wal->buffers[i].in_use, false);
        ppdb_sync_init(&wal->buffers[i].sync, &config->sync_config);
    }

    atomic_init(&wal->current_buffer, 0);
    atomic_init(&wal->next_sequence, 1);
    atomic_init(&wal->closed, false);
    atomic_init(&wal->file_size, 0);

    return wal;
}

// Destroy WAL
void ppdb_wal_destroy(ppdb_wal_t* wal) {
    if (!wal) return;

    // Mark as closed
    atomic_store(&wal->closed, true);

    // Wait for all buffer operations to complete
    for (size_t i = 0; i < atomic_load(&wal->buffer_count); i++) {
        while (atomic_load(&wal->buffers[i].in_use)) {
            sched_yield();
        }
        ppdb_sync_destroy(&wal->buffers[i].sync);
        free(wal->buffers[i].data);
    }

    ppdb_sync_destroy(&wal->sync);
    free(wal->buffers);
    free(wal->filename);
    close(wal->fd);
    free(wal);
}

// Append record
int ppdb_wal_append(ppdb_wal_t* wal, ppdb_wal_record_type_t type,
                    const void* key, size_t key_len,
                    const void* value, size_t value_len,
                    uint64_t sequence) {
    if (!wal || !key || (value_len > 0 && !value)) return PPDB_ERR_INVALID_ARG;
    if (atomic_load(&wal->closed)) return PPDB_ERR_CLOSED;

    // Calculate record size
    size_t record_size = sizeof(wal_record_header_t) + key_len + value_len;
    if (record_size > wal->config.buffer_size) return PPDB_ERR_TOO_LARGE;

    // Get current buffer
    size_t current = atomic_load(&wal->current_buffer);
    wal_buffer_t* buffer = &wal->buffers[current];

    // Try to get buffer lock
    if (!ppdb_sync_try_lock(&buffer->sync)) {
        return PPDB_ERR_BUSY;
    }

    // Check buffer space
    size_t used = atomic_load(&buffer->used);
    if (used + record_size > atomic_load(&buffer->size)) {
        // Switch to next buffer
        size_t next = (current + 1) % atomic_load(&wal->buffer_count);
        wal_buffer_t* next_buffer = &wal->buffers[next];

        // Wait for next buffer to be available
        while (atomic_load(&next_buffer->in_use)) {
            sched_yield();
        }

        // Flush current buffer
        if (used > 0) {
            ssize_t written = write(wal->fd, buffer->data, used);
            if (written != used) {
                ppdb_sync_unlock(&buffer->sync);
                return PPDB_ERR_IO;
            }
            atomic_fetch_add(&wal->file_size, written);
            atomic_store(&buffer->used, 0);
        }

        // Switch buffer
        atomic_store(&wal->current_buffer, next);
        ppdb_sync_unlock(&buffer->sync);
        buffer = next_buffer;
        
        if (!ppdb_sync_try_lock(&buffer->sync)) {
            return PPDB_ERR_BUSY;
        }
    }

    // Prepare record header
    wal_record_header_t header = {
        .type = type,
        .key_size = key_len,
        .value_size = value_len,
        .sequence = sequence ? sequence : atomic_fetch_add(&wal->next_sequence, 1),
        .checksum = 0  // TODO: Implement checksum
    };

    // Write record
    used = atomic_load(&buffer->used);
    memcpy(buffer->data + used, &header, sizeof(header));
    used += sizeof(header);
    memcpy(buffer->data + used, key, key_len);
    used += key_len;
    if (value_len > 0) {
        memcpy(buffer->data + used, value, value_len);
        used += value_len;
    }
    atomic_store(&buffer->used, used);

    ppdb_sync_unlock(&buffer->sync);
    return PPDB_OK;
}

// Sync to disk
int ppdb_wal_sync(ppdb_wal_t* wal) {
    if (!wal) return PPDB_ERR_INVALID_ARG;
    if (atomic_load(&wal->closed)) return PPDB_ERR_CLOSED;

    // Get current buffer
    size_t current = atomic_load(&wal->current_buffer);
    wal_buffer_t* buffer = &wal->buffers[current];

    // Get buffer lock
    ppdb_sync_lock(&buffer->sync);

    // Write data
    size_t used = atomic_load(&buffer->used);
    if (used > 0) {
        ssize_t written = write(wal->fd, buffer->data, used);
        if (written != used) {
            ppdb_sync_unlock(&buffer->sync);
            return PPDB_ERR_IO;
        }
        atomic_fetch_add(&wal->file_size, written);
        atomic_store(&buffer->used, 0);
    }

    // Sync to disk
    if (fsync(wal->fd) != 0) {
        ppdb_sync_unlock(&buffer->sync);
        return PPDB_ERR_IO;
    }

    ppdb_sync_unlock(&buffer->sync);
    return PPDB_OK;
}

// Create recovery iterator
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

// Destroy recovery iterator
void ppdb_wal_recovery_iter_destroy(ppdb_wal_recovery_iter_t* iter) {
    if (!iter) return;
    free(iter->buffer);
    free(iter);
}

// Check if recovery iterator is valid
bool ppdb_wal_recovery_iter_valid(ppdb_wal_recovery_iter_t* iter) {
    if (!iter || !iter->wal) return false;
    return iter->position < atomic_load(&iter->wal->file_size);
}

// Get next record
int ppdb_wal_recovery_iter_next(ppdb_wal_recovery_iter_t* iter,
                               ppdb_wal_record_type_t* type,
                               void** key, size_t* key_len,
                               void** value, size_t* value_len,
                               uint64_t* sequence) {
    if (!iter || !iter->wal || !type || !key || !key_len || !value || !value_len || !sequence) {
        return PPDB_ERR_INVALID_ARG;
    }

    // Read record header
    wal_record_header_t header;
    ssize_t n = pread(iter->wal->fd, &header, sizeof(header), iter->position);
    if (n != sizeof(header)) {
        return PPDB_ERR_IO;
    }

    // Allocate memory
    *key = malloc(header.key_size);
    if (!*key) return PPDB_ERR_NO_MEMORY;

    if (header.value_size > 0) {
        *value = malloc(header.value_size);
        if (!*value) {
            free(*key);
            return PPDB_ERR_NO_MEMORY;
        }
    } else {
        *value = NULL;
    }

    // Read key and value
    n = pread(iter->wal->fd, *key, header.key_size, 
              iter->position + sizeof(header));
    if (n != header.key_size) {
        free(*key);
        free(*value);
        return PPDB_ERR_IO;
    }

    if (header.value_size > 0) {
        n = pread(iter->wal->fd, *value, header.value_size,
                 iter->position + sizeof(header) + header.key_size);
        if (n != header.value_size) {
            free(*key);
            free(*value);
            return PPDB_ERR_IO;
        }
    }

    // Update position and return values
    iter->position += sizeof(header) + header.key_size + header.value_size;
    *type = header.type;
    *key_len = header.key_size;
    *value_len = header.value_size;
    *sequence = header.sequence;

    return PPDB_OK;
}
