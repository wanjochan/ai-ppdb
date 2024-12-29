#include <cosmopolitan.h>
#include "ppdb/wal_lockfree.h"
#include "ppdb/memtable_lockfree.h"
#include "ppdb/error.h"
#include "ppdb/defs.h"
#include "ppdb/memtable.h"
#include "ppdb/logger.h"
#include "ppdb/fs.h"

#define WAL_PATH_LENGTH 512  // 增加WAL路径长度限制
#define WAL_SEGMENT_ID_MAX 999999999  // 最大段ID限制

// WAL头部结构
typedef struct {
    uint32_t magic;      // 魔数
    uint32_t version;    // 版本号
    uint32_t segment_size;  // 段大小
    uint32_t reserved;   // 保留字段
} ppdb_wal_header_t;

// WAL记录头部结构
typedef struct {
    uint32_t type;       // 记录类型
    uint32_t key_size;   // 键大小
    uint32_t value_size; // 值大小
} ppdb_wal_record_header_t;

// WAL结构
typedef struct ppdb_wal_t {
    char dir_path[MAX_PATH_LENGTH];  // WAL目录路径
    size_t segment_size;             // 段大小
    bool sync_write;                 // 是否同步写入
    atomic_int current_fd;           // 当前文件描述符
    atomic_size_t current_size;      // 当前段大小
    atomic_size_t segment_id;        // 当前段ID
} ppdb_wal_t;

// 前向声明
static ppdb_error_t create_new_segment(ppdb_wal_t* wal);

// 创建WAL实例
ppdb_error_t ppdb_wal_create_lockfree(const ppdb_wal_config_t* config, ppdb_wal_t** wal) {
    if (!config || !wal || !config->dir_path || config->dir_path[0] == '\0') {
        ppdb_log_error("Invalid arguments: config=%p, wal=%p", config, wal);
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_log_info("Creating lock-free WAL at: %s", config->dir_path);

    // 分配WAL结构
    ppdb_wal_t* new_wal = (ppdb_wal_t*)calloc(1, sizeof(ppdb_wal_t));
    if (!new_wal) {
        ppdb_log_error("Failed to allocate WAL");
        return PPDB_ERR_NO_MEMORY;
    }

    // 初始化基本字段
    strncpy(new_wal->dir_path, config->dir_path, sizeof(new_wal->dir_path) - 1);
    new_wal->dir_path[sizeof(new_wal->dir_path) - 1] = '\0';  // 确保字符串结束
    new_wal->segment_size = config->segment_size;
    new_wal->sync_write = config->sync_write;
    atomic_init(&new_wal->current_fd, -1);
    atomic_init(&new_wal->current_size, 0);
    atomic_init(&new_wal->segment_id, 0);

    // 创建WAL目录
    if (!ppdb_fs_dir_exists(config->dir_path)) {
        if (ppdb_fs_mkdir(config->dir_path) != 0) {
            ppdb_log_error("Failed to create WAL directory: %s", config->dir_path);
            free(new_wal);
            return PPDB_ERR_IO;
        }
    }

    // 创建新的WAL段
    ppdb_error_t err = create_new_segment(new_wal);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create new WAL segment: %s", ppdb_error_string(err));
        free(new_wal);
        return err;
    }

    *wal = new_wal;
    return PPDB_OK;
}

void ppdb_wal_destroy_lockfree(ppdb_wal_t* wal) {
    if (!wal) return;

    // 关闭当前文件
    int fd = atomic_load(&wal->current_fd);
    if (fd >= 0) {
        if (wal->sync_write) {
            fsync(fd);
        }
        close(fd);
    }

    // 清零内存并释放
    memset(wal, 0, sizeof(ppdb_wal_t));
    free(wal);
}

void ppdb_wal_close_lockfree(ppdb_wal_t* wal) {
    if (!wal) return;

    // 关闭当前文件
    int fd = atomic_load(&wal->current_fd);
    if (fd >= 0) {
        if (wal->sync_write) {
            fsync(fd);
        }
        close(fd);
        atomic_store(&wal->current_fd, -1);
    }
}

ppdb_error_t ppdb_wal_write_lockfree(ppdb_wal_t* wal, ppdb_wal_record_type_t type,
                                    const void* key, size_t key_size,
                                    const void* value, size_t value_size) {
    if (!wal || !key || key_size == 0 || (type == PPDB_WAL_RECORD_PUT && (!value || value_size == 0))) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 检查段ID是否超出限制
    size_t current_id = atomic_load(&wal->segment_id);
    if (current_id >= WAL_SEGMENT_ID_MAX) {
        ppdb_log_error("WAL segment ID overflow: %zu", current_id);
        return PPDB_ERR_LIMIT_EXCEEDED;
    }

    ppdb_wal_record_header_t header = {
        .type = type,
        .key_size = key_size,
        .value_size = value_size
    };

    size_t record_size = sizeof(header) + key_size + value_size;
    size_t old_size, new_size;

    do {
        old_size = atomic_load(&wal->current_size);
        new_size = old_size + record_size;

        if (new_size > wal->segment_size) {
            // 需要创建新段
            char filename[WAL_PATH_LENGTH];
            size_t new_id = atomic_fetch_add(&wal->segment_id, 1) + 1;
            
            // 检查新段ID是否超出限制
            if (new_id > WAL_SEGMENT_ID_MAX) {
                ppdb_log_error("WAL segment ID overflow: %zu", new_id);
                return PPDB_ERR_LIMIT_EXCEEDED;
            }

            int n = snprintf(filename, sizeof(filename), "%s/%010zu.log", wal->dir_path, new_id);
            if (n < 0 || (size_t)n >= sizeof(filename)) {
                ppdb_log_error("WAL filename too long");
                return PPDB_ERR_PATH_TOO_LONG;
            }

            int new_fd = open(filename, O_RDWR | O_CREAT, 0644);
            if (new_fd == -1) return PPDB_ERR_IO;

            ppdb_wal_header_t wal_header = {
                .magic = WAL_MAGIC,
                .version = WAL_VERSION,
                .segment_size = wal->segment_size,
                .reserved = 0
            };

            ssize_t written = write(new_fd, &wal_header, sizeof(wal_header));
            if (written < 0 || (size_t)written != sizeof(wal_header)) {
                close(new_fd);
                return PPDB_ERR_IO;
            }

            int old_fd = atomic_exchange(&wal->current_fd, new_fd);
            if (old_fd >= 0) {
                fsync(old_fd);
                close(old_fd);
            }

            atomic_store(&wal->current_size, sizeof(wal_header));
            old_size = sizeof(wal_header);
            new_size = old_size + record_size;
        }
    } while (!atomic_compare_exchange_weak(&wal->current_size, &old_size, new_size));

    int fd = atomic_load(&wal->current_fd);
    if (fd < 0) return PPDB_ERR_IO;

    // 写入记录
    ssize_t written;
    if ((written = write(fd, &header, sizeof(header))) < 0 || (size_t)written != sizeof(header)) return PPDB_ERR_IO;
    if ((written = write(fd, key, key_size)) < 0 || (size_t)written != key_size) return PPDB_ERR_IO;
    if (type == PPDB_WAL_RECORD_PUT && ((written = write(fd, value, value_size)) < 0 || (size_t)written != value_size)) return PPDB_ERR_IO;

    if (wal->sync_write) fsync(fd);

    return PPDB_OK;
}

ppdb_error_t ppdb_wal_recover_lockfree(ppdb_wal_t* wal, ppdb_memtable_t** table) {
    if (!wal || !table) {
        ppdb_log_error("Invalid arguments: wal=%p, table=%p", wal, table);
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_log_info("Recovering WAL from: %s", wal->dir_path);

    // 获取WAL目录中的所有文件
    DIR* dir = opendir(wal->dir_path);
    if (!dir) {
        if (errno == ENOENT) {
            ppdb_log_info("WAL directory does not exist, skipping recovery");
            return PPDB_OK;  // 目录不存在，视为成功
        }
        ppdb_log_error("Failed to open WAL directory: %s", strerror(errno));
        return PPDB_ERR_IO;
    }

    struct dirent* entry;
    ppdb_error_t err = PPDB_OK;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG || !strstr(entry->d_name, ".log")) continue;

        // 检查文件名长度
        size_t name_len = strlen(entry->d_name);
        if (strlen(wal->dir_path) + name_len + 2 > MAX_PATH_LENGTH) {
            ppdb_log_error("Path too long for file: %s", entry->d_name);
            continue;
        }

        char path[MAX_PATH_LENGTH];
        int n = snprintf(path, sizeof(path), "%s/%s", wal->dir_path, entry->d_name);
        if (n < 0 || (size_t)n >= sizeof(path)) {
            ppdb_log_error("Path too long for file: %s", entry->d_name);
            continue;
        }

        int fd = open(path, O_RDONLY);
        if (fd == -1) {
            ppdb_log_error("Failed to open WAL file: %s", strerror(errno));
            continue;
        }

        ppdb_wal_header_t header;
        ssize_t read_size = read(fd, &header, sizeof(header));
        if (read_size < 0 || (size_t)read_size != sizeof(header) || header.magic != WAL_MAGIC) {
            ppdb_log_error("Invalid WAL header in file: %s", path);
            close(fd);
            continue;
        }

        ppdb_log_info("Processing WAL file: %s", path);
        size_t processed_records = 0;

        while (1) {
            ppdb_wal_record_header_t record;
            if (read(fd, &record, sizeof(record)) != sizeof(record)) break;

            // 验证记录头部
            if (record.key_size == 0 || record.key_size > MAX_KEY_SIZE ||
                record.value_size > MAX_VALUE_SIZE) {
                ppdb_log_error("Invalid record header: key_size=%u, value_size=%u",
                             record.key_size, record.value_size);
                err = PPDB_ERR_CORRUPTED;
                close(fd);
                goto cleanup;
            }

            uint8_t* key = malloc(record.key_size);
            uint8_t* value = NULL;

            if (!key) {
                ppdb_log_error("Failed to allocate key buffer");
                close(fd);
                err = PPDB_ERR_NO_MEMORY;
                goto cleanup;
            }

            if (read(fd, key, record.key_size) != record.key_size) {
                ppdb_log_error("Failed to read key");
                free(key);
                close(fd);
                err = PPDB_ERR_IO;
                goto cleanup;
            }

            if (record.type == PPDB_WAL_RECORD_PUT) {
                value = malloc(record.value_size);
                if (!value) {
                    ppdb_log_error("Failed to allocate value buffer");
                    free(key);
                    close(fd);
                    err = PPDB_ERR_NO_MEMORY;
                    goto cleanup;
                }

                if (read(fd, value, record.value_size) != record.value_size) {
                    ppdb_log_error("Failed to read value");
                    free(key);
                    free(value);
                    close(fd);
                    err = PPDB_ERR_IO;
                    goto cleanup;
                }

                err = ppdb_memtable_put_lockfree(*table, key, record.key_size,
                                                value, record.value_size);
            } else {
                err = ppdb_memtable_delete_lockfree(*table, key, record.key_size);
            }

            free(key);
            free(value);

            if (err != PPDB_OK && err != PPDB_ERR_NOT_FOUND) {
                ppdb_log_error("Failed to apply record: %s", ppdb_error_string(err));
                close(fd);
                goto cleanup;
            }

            processed_records++;
        }

        ppdb_log_info("Processed %zu records from WAL file: %s", processed_records, path);
        close(fd);
    }

cleanup:
    closedir(dir);

    if (err == PPDB_OK) {
        ppdb_log_info("WAL recovery completed successfully");
    } else {
        ppdb_log_error("WAL recovery failed with error: %s", ppdb_error_string(err));
    }
    return err;
}

ppdb_error_t ppdb_wal_archive_lockfree(ppdb_wal_t* wal) {
    if (!wal) return PPDB_ERR_INVALID_ARG;

    size_t current_id = atomic_load(&wal->segment_id);
    if (current_id == 0) return PPDB_OK;  // 没有需要归档的文件

    DIR* dir = opendir(wal->dir_path);
    if (!dir) return PPDB_ERR_IO;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG || !strstr(entry->d_name, ".log")) continue;

        char* end;
        unsigned long id = strtoul(entry->d_name, &end, 10);
        if (end == entry->d_name || id >= current_id) continue;

        // 检查文件名长度
        size_t name_len = strlen(entry->d_name);
        if (strlen(wal->dir_path) + name_len + 2 > WAL_PATH_LENGTH) {
            ppdb_log_error("Path too long for file: %s", entry->d_name);
            continue;
        }

        char path[WAL_PATH_LENGTH];
        int n = snprintf(path, sizeof(path), "%s/%s", wal->dir_path, entry->d_name);
        if (n < 0 || (size_t)n >= sizeof(path)) {
            ppdb_log_error("Path too long for file: %s", entry->d_name);
            continue;
        }
        unlink(path);
    }

    closedir(dir);
    return PPDB_OK;
}

static ppdb_error_t create_new_segment(ppdb_wal_t* wal) {
    if (!wal) return PPDB_ERR_NULL_POINTER;

    // 如果当前文件已打开，则关闭
    int old_fd = atomic_load(&wal->current_fd);
    if (old_fd >= 0) {
        if (wal->sync_write) {
            fsync(old_fd);
        }
        close(old_fd);
        atomic_store(&wal->current_fd, -1);
    }

    // 生成新的段文件名
    char segment_path[MAX_PATH_LENGTH];
    size_t dir_len = strlen(wal->dir_path);
    
    // 安全检查：确保路径长度不会溢出
    if (dir_len > MAX_PATH_LENGTH - 20) {  // 20 = len("/%09zu.log") + 1
        ppdb_log_error("WAL directory path too long");
        return PPDB_ERR_PATH_TOO_LONG;
    }

    size_t current_id = atomic_load(&wal->segment_id);
    // 检查段ID是否超出限制
    if (current_id >= WAL_SEGMENT_ID_MAX) {
        ppdb_log_error("WAL segment ID overflow: %zu", current_id);
        return PPDB_ERR_LIMIT_EXCEEDED;
    }
    
    ssize_t path_len = snprintf(segment_path, sizeof(segment_path), WAL_SEGMENT_NAME_FMT,
                               wal->dir_path, current_id);
    
    if (path_len < 0 || (size_t)path_len >= sizeof(segment_path)) {
        ppdb_log_error("Failed to construct WAL segment path");
        return PPDB_ERR_PATH_TOO_LONG;
    }

    // 创建新的段文件
    int fd = open(segment_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        ppdb_log_error("Failed to create WAL segment: %s", segment_path);
        return PPDB_ERR_IO;
    }

    // 写入WAL头部
    ppdb_wal_header_t header = {
        .magic = WAL_MAGIC,
        .version = WAL_VERSION,
        .segment_size = (uint32_t)wal->segment_size,
        .reserved = 0
    };

    ssize_t bytes_written = write(fd, &header, sizeof(header));
    if (bytes_written != sizeof(header)) {
        ppdb_log_error("Failed to write WAL header");
        close(fd);
        return PPDB_ERR_IO;
    }

    // 如果需要同步写入
    if (wal->sync_write) {
        if (fsync(fd) != 0) {
            ppdb_log_error("Failed to sync WAL header");
            close(fd);
            return PPDB_ERR_IO;
        }
    }

    // 更新WAL状态
    atomic_store(&wal->current_fd, fd);
    atomic_store(&wal->current_size, sizeof(header));
    atomic_fetch_add(&wal->segment_id, 1);

    return PPDB_OK;
} 