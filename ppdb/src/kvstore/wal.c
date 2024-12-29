#include <cosmopolitan.h>
#include "internal/wal.h"
#include "internal/sync.h"
#include <fcntl.h>
#include <unistd.h>

// WAL record header
typedef struct wal_record_header {
    uint32_t type;           // Record type
    uint32_t key_size;       // Key size
    uint32_t value_size;     // Value size
    uint64_t sequence;       // Sequence number
    uint32_t checksum;       // Checksum
} wal_record_header_t;

// WAL buffer
typedef struct wal_buffer {
    char* data;              // Data
    size_t size;            // Size
    size_t used;            // Used size
    bool in_use;            // Whether in use
} wal_buffer_t;

// WAL structure
struct ppdb_wal {
    int fd;                  // File descriptor
    char* filename;          // Filename
    ppdb_wal_config_t config;// Configuration
    ppdb_sync_t sync;       // Synchronization
    wal_buffer_t* buffers;  // Buffer array
    size_t buffer_count;    // Buffer count
    size_t current_buffer;  // Current buffer
    wal_group_t group;      // Group commit
    bool enable_group_commit;// Enable group commit
    uint32_t group_interval; // Group commit interval
    bool enable_async_flush; // Enable async flush
    bool enable_checksum;    // Enable checksum
    uint64_t file_size;     // File size
    bool closed;             // Whether closed
};

// WAL recovery iterator
struct ppdb_wal_recovery_iter {
    ppdb_wal_t* wal;        // WAL pointer
    off_t position;         // Current position
    char* buffer;           // Read buffer
    size_t buffer_size;     // Buffer size
};

// Calculate checksum
static uint32_t calculate_checksum(const void* data, size_t len) {
    // TODO: Implement CRC32
    return 0;
}

// Create WAL
ppdb_wal_t* ppdb_wal_create(const char* filename, const ppdb_wal_config_t* config) {
    if (!filename || !config) return NULL;

    ppdb_wal_t* wal = (ppdb_wal_t*)malloc(sizeof(ppdb_wal_t));
    if (!wal) return NULL;

    // Initialize synchronization
    if (ppdb_sync_init(&wal->sync, &config->sync_config) != 0) {
        free(wal);
        return NULL;
    }

    // Open file
    wal->fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (wal->fd < 0) {
        ppdb_sync_destroy(&wal->sync);
        free(wal);
        return NULL;
    }

    // Initialize buffers
    wal->buffer_count = 2;  // Double buffer
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
    wal->closed = false;

    return wal;
}

// Destroy WAL
void ppdb_wal_destroy(ppdb_wal_t* wal) {
    if (!wal) return;

    // Flush all data
    ppdb_wal_sync(wal);

    // Release resources
    for (size_t i = 0; i < wal->buffer_count; i++) {
        free(wal->buffers[i].data);
    }
    free(wal->buffers);
    close(wal->fd);
    ppdb_sync_destroy(&wal->sync);
    free(wal);
}

// Append record
int ppdb_wal_append(ppdb_wal_t* wal, ppdb_wal_record_type_t type,
                    const void* key, size_t key_len,
                    const void* value, size_t value_len,
                    uint64_t sequence) {
    if (!wal || !key || !value) return -1;

    ppdb_sync_lock(&wal->sync);

    if (wal->closed) {
        ppdb_sync_unlock(&wal->sync);
        return -1;
    }

    // Calculate record size
    size_t record_size = sizeof(wal_record_header_t) + key_len + value_len;

    // Get available buffer
    wal_buffer_t* buffer = NULL;
    for (size_t i = 0; i < wal->buffer_count; i++) {
        if (!wal->buffers[i].in_use && wal->buffers[i].size >= record_size) {
            buffer = &wal->buffers[i];
            buffer->in_use = true;
            break;
        }
    }

    if (!buffer) {
        ppdb_sync_unlock(&wal->sync);
        return -1;
    }

    // Write record header
    wal_record_header_t header = {
        .type = type,
        .key_size = key_len,
        .value_size = value_len,
        .sequence = sequence,
        .checksum = 0
    };

    if (wal->enable_checksum) {
        header.checksum = calculate_checksum(key, key_len);
        if (value) {
            header.checksum ^= calculate_checksum(value, value_len);
        }
    }

    memcpy(buffer->data + buffer->used, &header, sizeof(header));
    buffer->used += sizeof(header);

    // Write key and value
    memcpy(buffer->data + buffer->used, key, key_len);
    buffer->used += key_len;

    if (value) {
        memcpy(buffer->data + buffer->used, value, value_len);
        buffer->used += value_len;
    }

    // Sync if needed
    if (!wal->enable_async_flush) {
        if (write(wal->fd, buffer->data, buffer->used) != buffer->used) {
            buffer->in_use = false;
            buffer->used = 0;
            ppdb_sync_unlock(&wal->sync);
            return -1;
        }
        
        if (wal->config.sync_write) {
            fsync(wal->fd);
        }
    }

    // Update file size
    wal->file_size += buffer->used;

    // Release buffer
    buffer->in_use = false;
    buffer->used = 0;

    ppdb_sync_unlock(&wal->sync);
    return 0;
}

// Sync to disk
int ppdb_wal_sync(ppdb_wal_t* wal) {
    if (!wal) return -1;

    ppdb_sync_lock(&wal->sync);

    // Write all buffers
    for (size_t i = 0; i < wal->buffer_count; i++) {
        wal_buffer_t* buffer = &wal->buffers[i];
        if (buffer->used > 0) {
            ssize_t written = write(wal->fd, buffer->data, buffer->used);
            if (written != buffer->used) {
                ppdb_sync_unlock(&wal->sync);
                return -1;
            }
            buffer->used = 0;
            buffer->in_use = false;
        }
    }

    // Sync to disk
    if (!wal->enable_async_flush) {
        fsync(wal->fd);
    }

    // Reset group commit
    wal->group.count = 0;
    wal->group.committing = false;

    ppdb_sync_unlock(&wal->sync);
    return 0;
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
    return iter->position < iter->wal->file_size;
}

// Get next record
int ppdb_wal_recovery_iter_next(ppdb_wal_recovery_iter_t* iter,
                               ppdb_wal_record_type_t* type,
                               void** key, size_t* key_len,
                               void** value, size_t* value_len,
                               uint64_t* sequence) {
    if (!iter || !iter->wal || !type || !key || !key_len || !value || !value_len || !sequence) {
        return -1;
    }

    // Read record header
    wal_record_header_t header;
    ssize_t read_size = pread(iter->wal->fd, &header, sizeof(header), iter->position);
    if (read_size != sizeof(header)) {
        return -1;
    }

    // Validate record
    if (header.key_size > iter->wal->config.max_record_size ||
        header.value_size > iter->wal->config.max_record_size) {
        return -1;
    }

    // Read key
    *key = malloc(header.key_size);
    if (!*key) return -1;
    read_size = pread(iter->wal->fd, *key, header.key_size, 
                     iter->position + sizeof(header));
    if (read_size != header.key_size) {
        free(*key);
        return -1;
    }

    // Read value if exists
    if (header.value_size > 0) {
        *value = malloc(header.value_size);
        if (!*value) {
            free(*key);
            return -1;
        }
        read_size = pread(iter->wal->fd, *value, header.value_size,
                         iter->position + sizeof(header) + header.key_size);
        if (read_size != header.value_size) {
            free(*key);
            free(*value);
            return -1;
        }
    } else {
        *value = NULL;
    }

    // Verify checksum
    uint32_t checksum = calculate_checksum(*key, header.key_size);
    if (header.value_size > 0) {
        checksum ^= calculate_checksum(*value, header.value_size);
    }
    if (checksum != header.checksum) {
        free(*key);
        if (*value) free(*value);
        return -1;
    }

    // Update output parameters
    *type = header.type;
    *key_len = header.key_size;
    *value_len = header.value_size;
    *sequence = header.sequence;

    // Move to next record
    iter->position += sizeof(header) + header.key_size + header.value_size;

    return 0;
}
