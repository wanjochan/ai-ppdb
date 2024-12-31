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

    // 生成WAL文件名
    char filename[256];
    if (config->filename[0] != '\0') {
        snprintf(filename, sizeof(filename), "%s/%s", new_wal->dir_path, config->filename);
    } else {
        snprintf(filename, sizeof(filename), "%s/wal.log", new_wal->dir_path);
    }
    new_wal->filename = strdup(filename);
    if (!new_wal->filename) {
        free(new_wal->dir_path);
        free(new_wal);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 设置同步模式
    new_wal->sync_on_write = config->sync_write;

    // 创建目录
    if (mkdir(new_wal->dir_path, 0755) != 0 && errno != EEXIST) {
        free(new_wal->filename);
        free(new_wal->dir_path);
        free(new_wal);
        return PPDB_ERR_IO;
    }

    // 打开WAL文件
    new_wal->current_fd = open(new_wal->filename, O_CREAT | O_RDWR | O_APPEND, 0644);
    if (new_wal->current_fd < 0) {
        free(new_wal->filename);
        free(new_wal->dir_path);
        free(new_wal);
        return PPDB_ERR_IO;
    }

    // 获取文件大小
    off_t file_size = lseek(new_wal->current_fd, 0, SEEK_END);
    if (file_size < 0) {
        close(new_wal->current_fd);
        free(new_wal->filename);
        free(new_wal->dir_path);
        free(new_wal);
        return PPDB_ERR_IO;
    }

    // 如果是新文件，写入头部
    if (file_size == 0) {
        if (write(new_wal->current_fd, &header, sizeof(header)) != sizeof(header)) {
            close(new_wal->current_fd);
            free(new_wal->filename);
            free(new_wal->dir_path);
            free(new_wal);
            return PPDB_ERR_IO;
        }
        new_wal->current_size = sizeof(header);
    } else {
        // 读取现有头部
        if (pread(new_wal->current_fd, &header, sizeof(header), 0) != sizeof(header)) {
            close(new_wal->current_fd);
            free(new_wal->filename);
            free(new_wal->dir_path);
            free(new_wal);
            return PPDB_ERR_IO;
        }
        new_wal->current_size = file_size;
    }

    // 初始化其他字段
    new_wal->next_sequence = header.sequence;
    new_wal->sync_on_write = config->sync_write;
    new_wal->closed = false;

    // 初始化同步对象
    new_wal->sync = (ppdb_sync_t*)malloc(sizeof(ppdb_sync_t));
    if (!new_wal->sync) {
        close(new_wal->current_fd);
        free(new_wal->filename);
        free(new_wal->dir_path);
        free(new_wal);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    ppdb_sync_config_t sync_config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 1000
    };

    ppdb_error_t err = ppdb_sync_init(new_wal->sync, &sync_config);
    if (err != PPDB_OK) {
        free(new_wal->sync);
        close(new_wal->current_fd);
        free(new_wal->filename);
        free(new_wal->dir_path);
        free(new_wal);
        return err;
    }

    *wal = new_wal;
    return PPDB_OK;
}

void ppdb_wal_destroy_basic(ppdb_wal_t* wal) {
    if (!wal) {
        return;
    }

    if (wal->current_fd >= 0) {
        close(wal->current_fd);
    }

    if (wal->sync) {
        ppdb_sync_destroy(wal->sync);
        free(wal->sync);
    }

    free(wal->filename);
    free(wal->dir_path);
    free(wal);
}

ppdb_error_t ppdb_wal_write_basic(ppdb_wal_t* wal, const void* key, size_t key_len,
                                 const void* value, size_t value_len) {
    if (!wal || !key || !value) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (wal->closed) {
        return PPDB_ERR_WAL_CLOSED;
    }

    // 构造记录头部
    wal_record_header_t record_header = {
        .type = PPDB_WAL_RECORD_PUT,
        .key_size = key_len,
        .value_size = value_len,
        .crc32 = 0
    };

    // 计算CRC32
    record_header.crc32 = calculate_crc32(key, key_len);
    if (value && value_len > 0) {
        record_header.crc32 ^= calculate_crc32(value, value_len);
    }

    // 加锁写入
    ppdb_sync_lock(wal->sync);

    // 写入记录头部
    if (write(wal->current_fd, &record_header, sizeof(record_header)) != sizeof(record_header)) {
        ppdb_sync_unlock(wal->sync);
        return PPDB_ERR_IO;
    }

    // 写入键
    if (write(wal->current_fd, key, key_len) != key_len) {
        ppdb_sync_unlock(wal->sync);
        return PPDB_ERR_IO;
    }

    // 写入值
    if (value && value_len > 0) {
        if (write(wal->current_fd, value, value_len) != value_len) {
            ppdb_sync_unlock(wal->sync);
            return PPDB_ERR_IO;
        }
    }

    // 更新文件大小
    wal->current_size += sizeof(record_header) + key_len + value_len;

    // 如果需要同步
    if (wal->sync_on_write) {
        if (fsync(wal->current_fd) != 0) {
            ppdb_sync_unlock(wal->sync);
            return PPDB_ERR_IO;
        }
    }

    ppdb_sync_unlock(wal->sync);
    return PPDB_OK;
}

ppdb_error_t ppdb_wal_sync_basic(ppdb_wal_t* wal) {
    if (!wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (wal->closed) {
        return PPDB_ERR_WAL_CLOSED;
    }

    ppdb_sync_lock(wal->sync);
    
    if (fsync(wal->current_fd) != 0) {
        ppdb_sync_unlock(wal->sync);
        return PPDB_ERR_IO;
    }

    ppdb_sync_unlock(wal->sync);
    return PPDB_OK;
}

size_t ppdb_wal_size_basic(ppdb_wal_t* wal) {
    if (!wal) {
        return 0;
    }
    return wal->current_size;
}

uint64_t ppdb_wal_next_sequence_basic(ppdb_wal_t* wal) {
    if (!wal) {
        return 0;
    }
    return wal->next_sequence++;
}

// 工厂函数实现
ppdb_error_t ppdb_wal_create(const ppdb_wal_config_t* config, ppdb_wal_t** wal) {
    return ppdb_wal_create_basic(config, wal);
}

void ppdb_wal_destroy(ppdb_wal_t* wal) {
    ppdb_wal_destroy_basic(wal);
}

ppdb_error_t ppdb_wal_write(ppdb_wal_t* wal, const void* key, size_t key_len,
                           const void* value, size_t value_len) {
    return ppdb_wal_write_basic(wal, key, key_len, value, value_len);
}

ppdb_error_t ppdb_wal_sync(ppdb_wal_t* wal) {
    return ppdb_wal_sync_basic(wal);
}

size_t ppdb_wal_size(ppdb_wal_t* wal) {
    return ppdb_wal_size_basic(wal);
}

uint64_t ppdb_wal_next_sequence(ppdb_wal_t* wal) {
    return ppdb_wal_next_sequence_basic(wal);
}

// 关闭WAL
void ppdb_wal_close(ppdb_wal_t* wal) {
    if (!wal) {
        return;
    }

    // 同步并关闭文件
    if (wal->current_fd >= 0) {
        ppdb_wal_sync(wal);
        close(wal->current_fd);
        wal->current_fd = -1;
    }

    wal->closed = true;
}
