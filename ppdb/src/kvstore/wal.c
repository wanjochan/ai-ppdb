#include <cosmopolitan.h>
#include "ppdb/wal.h"
#include "ppdb/error.h"
#include "ppdb/defs.h"
#include "../common/logger.h"
#include "../common/fs.h"

#define WAL_MAGIC 0x4C415750  // "PWAL"
#define WAL_VERSION 1

// 前向声明
static ppdb_error_t create_new_segment(ppdb_wal_t* wal);
static ppdb_error_t archive_old_wal_files(const char* wal_dir);

// 创建WAL实例
ppdb_error_t ppdb_wal_create(const ppdb_wal_config_t* config,
                            ppdb_wal_t** wal) {
    if (!config || !config->dir_path || !wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_log_info("Creating WAL at: %s", config->dir_path);

    // 确保目录存在
    ppdb_error_t err = ppdb_ensure_directory(config->dir_path);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create WAL directory: %s", config->dir_path);
        return err;
    }

    // 分配WAL实例
    ppdb_wal_t* new_wal = (ppdb_wal_t*)calloc(1, sizeof(ppdb_wal_t));
    if (!new_wal) {
        return PPDB_ERR_NO_MEMORY;
    }

    // 初始化WAL实例
    strncpy(new_wal->dir_path, config->dir_path, MAX_PATH_LENGTH - 1);
    new_wal->dir_path[MAX_PATH_LENGTH - 1] = '\0';
    new_wal->segment_size = config->segment_size;
    new_wal->sync_write = config->sync_write;
    new_wal->current_fd = -1;

    // 创建新的WAL段
    err = create_new_segment(new_wal);
    if (err != PPDB_OK) {
        free(new_wal);
        return err;
    }

    ppdb_log_info("Successfully created WAL at: %s", config->dir_path);
    *wal = new_wal;
    return PPDB_OK;
}

// 销毁WAL实例
void ppdb_wal_destroy(ppdb_wal_t* wal) {
    if (!wal) {
        return;
    }

    ppdb_log_info("Destroying WAL at: %s", wal->dir_path);
    if (wal->current_fd >= 0) {
        close(wal->current_fd);
    }
    free(wal);
}

// 创建新的WAL段文件
static ppdb_error_t create_new_segment(ppdb_wal_t* wal) {
    char filename[MAX_PATH_LENGTH];
    int timestamp = (int)time(NULL);
    int written = snprintf(filename, sizeof(filename), "%s/wal.%d", 
                          wal->dir_path, timestamp);
    if (written < 0 || written >= sizeof(filename)) {
        ppdb_log_error("Failed to format WAL filename: path too long");
        return PPDB_ERR_IO;
    }

    ppdb_log_debug("Creating new WAL segment: %s", filename);

    // 创建新文件
    int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        ppdb_log_error("Failed to create WAL segment: %s, error: %s", filename, strerror(errno));
        return PPDB_ERR_IO;
    }

    // 写入文件头
    ppdb_wal_header_t header = {
        .magic = WAL_MAGIC,
        .version = WAL_VERSION,
        .segment_size = wal->segment_size,
        .reserved = 0
    };

    if (write(fd, &header, sizeof(header)) != sizeof(header)) {
        ppdb_log_error("Failed to write WAL header: %s, error: %s", filename, strerror(errno));
        close(fd);
        return PPDB_ERR_IO;
    }

    // 关闭旧文件
    if (wal->current_fd >= 0) {
        close(wal->current_fd);
    }

    wal->current_fd = fd;
    ppdb_log_debug("Successfully created WAL segment: %s", filename);
    return PPDB_OK;
}

// 写入记录
ppdb_error_t ppdb_wal_write(ppdb_wal_t* wal,
                           ppdb_wal_record_type_t type,
                           const void* key,
                           size_t key_size,
                           const void* value,
                           size_t value_size) {
    if (!wal || !key || key_size == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (type == PPDB_WAL_RECORD_PUT && (!value || value_size == 0)) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 准备记录头
    ppdb_wal_record_header_t header = {
        .type = (uint32_t)type,
        .key_size = key_size,
        .value_size = value_size
    };

    ppdb_log_debug("Writing WAL record: type=%d, key_size=%zu, value_size=%zu", 
                   type, key_size, value_size);

    // 写入记录头
    ssize_t written = write(wal->current_fd, &header, sizeof(header));
    if (written != sizeof(header)) {
        ppdb_log_error("Failed to write WAL record header: %s", strerror(errno));
        return PPDB_ERR_IO;
    }

    // 写入键
    written = write(wal->current_fd, key, key_size);
    if ((size_t)written != key_size) {
        ppdb_log_error("Failed to write WAL key: %s", strerror(errno));
        return PPDB_ERR_IO;
    }

    // 写入值
    if (value && value_size > 0) {
        written = write(wal->current_fd, value, value_size);
        if ((size_t)written != value_size) {
            ppdb_log_error("Failed to write WAL value: %s", strerror(errno));
            return PPDB_ERR_IO;
        }
    }

    // 同步写入
    if (wal->sync_write) {
        if (fsync(wal->current_fd) != 0) {
            ppdb_log_error("Failed to sync WAL to disk: %s", strerror(errno));
            return PPDB_ERR_IO;
        }
    }

    return PPDB_OK;
}

// 恢复数据
ppdb_error_t ppdb_wal_recover(ppdb_wal_t* wal,
                             ppdb_memtable_t* table) {
    if (!wal || !table) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_log_info("Recovering WAL from: %s", wal->dir_path);

    DIR* dir = opendir(wal->dir_path);
    if (!dir) {
        ppdb_log_error("Failed to open WAL directory: %s, error: %s", wal->dir_path, strerror(errno));
        return PPDB_ERR_IO;
    }

    struct dirent* entry;
    bool found_wal = false;
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "wal.", 4) != 0) {
            continue;
        }

        found_wal = true;
        char filename[MAX_PATH_LENGTH];
        int written = snprintf(filename, sizeof(filename), "%s/%s", 
                             wal->dir_path, entry->d_name);
        if (written < 0 || written >= sizeof(filename)) {
            ppdb_log_error("Failed to format WAL filename: path too long");
            closedir(dir);
            return PPDB_ERR_IO;
        }

        // 打开WAL文件
        int fd = open(filename, O_RDONLY | O_CLOEXEC);
        if (fd < 0) {
            ppdb_log_error("Failed to open WAL file: %s, error: %s", filename, strerror(errno));
            continue;
        }

        // 读取文件头
        ppdb_wal_header_t header;
        ssize_t read_size = read(fd, &header, sizeof(header));
        if (read_size != sizeof(header)) {
            ppdb_log_error("Failed to read WAL header: %s", filename);
            close(fd);
            continue;
        }

        // 验证文件头
        if (header.magic != WAL_MAGIC || header.version != WAL_VERSION) {
            ppdb_log_error("Invalid WAL header: %s", filename);
            close(fd);
            continue;
        }

        // 读取记录
        while (1) {
            ppdb_wal_record_header_t record;
            read_size = read(fd, &record, sizeof(record));
            if (read_size == 0) {
                break;  // 文件结束
            }
            if (read_size != sizeof(record)) {
                ppdb_log_error("Failed to read WAL record header: %s", filename);
                break;
            }

            // 验证记录类型
            if (record.type != PPDB_WAL_RECORD_PUT && record.type != PPDB_WAL_RECORD_DELETE) {
                ppdb_log_error("Invalid WAL record type: %u", record.type);
                break;
            }

            // 读取键，为结尾空字符分配额外的字节
            uint8_t* key = malloc(record.key_size + 1);
            if (!key) {
                ppdb_log_error("Failed to allocate memory for key");
                break;
            }

            read_size = read(fd, key, record.key_size);
            if ((size_t)read_size != record.key_size) {
                ppdb_log_error("Failed to read WAL record key: %s", filename);
                free(key);
                break;
            }
            key[record.key_size] = '\0';  // 添加结尾空字符

            // 读取值（如果有），为结尾空字符分配额外的字节
            uint8_t* value = NULL;
            if (record.type == PPDB_WAL_RECORD_PUT) {
                value = malloc(record.value_size + 1);
                if (!value) {
                    ppdb_log_error("Failed to allocate memory for value");
                    free(key);
                    break;
                }

                read_size = read(fd, value, record.value_size);
                if ((size_t)read_size != record.value_size) {
                    ppdb_log_error("Failed to read WAL record value: %s", filename);
                    free(key);
                    free(value);
                    break;
                }
                value[record.value_size] = '\0';  // 添加结尾空字符
            }

            // 应用记录
            ppdb_error_t err;
            if (record.type == PPDB_WAL_RECORD_PUT) {
                err = ppdb_memtable_put(table, key, record.key_size + 1, value, record.value_size + 1);
            } else {
                err = ppdb_memtable_delete(table, key, record.key_size + 1);
            }

            free(key);
            if (value) {
                free(value);
            }

            if (err != PPDB_OK) {
                ppdb_log_error("Failed to apply WAL record: %s", filename);
                break;
            }
        }

        close(fd);
    }

    closedir(dir);
    return found_wal ? PPDB_OK : PPDB_ERR_NOT_FOUND;
}

// 归档旧的WAL文件
static ppdb_error_t archive_old_wal_files(const char* wal_dir) {
    char archive_dir[MAX_PATH_LENGTH];
    int written = snprintf(archive_dir, sizeof(archive_dir), "%s/archive", wal_dir);
    if (written < 0 || written >= sizeof(archive_dir)) {
        ppdb_log_error("Failed to format archive directory path: path too long");
        return PPDB_ERR_IO;
    }

    // 创建归档目录
    if (mkdir(archive_dir, 0755) != 0 && errno != EEXIST) {
        ppdb_log_error("Failed to create WAL archive directory: %s, error: %s", archive_dir, strerror(errno));
        return PPDB_ERR_IO;
    }

    DIR* dir = opendir(wal_dir);
    if (!dir) {
        ppdb_log_error("Failed to open WAL directory: %s, error: %s", wal_dir, strerror(errno));
        return PPDB_ERR_IO;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 ||
            strcmp(entry->d_name, "archive") == 0) {
            continue;
        }

        char old_path[MAX_PATH_LENGTH];
        char archive_path[MAX_PATH_LENGTH];
        written = snprintf(old_path, sizeof(old_path), "%s/%s", 
                         wal_dir, entry->d_name);
        if (written < 0 || written >= sizeof(old_path)) {
            ppdb_log_warn("Failed to format old path: path too long");
            continue;
        }

        written = snprintf(archive_path, sizeof(archive_path), "%s/%s", 
                         archive_dir, entry->d_name);
        if (written < 0 || written >= sizeof(archive_path)) {
            ppdb_log_warn("Failed to format archive path: path too long");
            continue;
        }

        if (rename(old_path, archive_path) != 0) {
            ppdb_log_warn("Failed to archive WAL file: %s, error: %s", old_path, strerror(errno));
        } else {
            ppdb_log_debug("Archived WAL file: %s -> %s", old_path, archive_path);
        }
    }

    closedir(dir);
    return PPDB_OK;
}

// 归档WAL文件
ppdb_error_t ppdb_wal_archive(ppdb_wal_t* wal) {
    if (!wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_log_info("Archiving WAL files in: %s", wal->dir_path);
    return archive_old_wal_files(wal->dir_path);
}