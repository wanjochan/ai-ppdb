#include <cosmopolitan.h>
#include "ppdb/wal.h"
#include "ppdb/error.h"
#include "ppdb/defs.h"
#include "../common/logger.h"
#include "../common/fs.h"

// 前向声明
static ppdb_error_t create_new_segment(ppdb_wal_t* wal);
static ppdb_error_t archive_old_wal_files(const char* wal_dir);

// 创建WAL实例
ppdb_error_t ppdb_wal_create(const ppdb_wal_config_t* config, ppdb_wal_t** wal) {
    if (!config || !wal || !config->dir_path || config->dir_path[0] == '\0') {
        ppdb_log_error("Invalid arguments: config=%p, wal=%p", config, wal);
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_log_info("Creating WAL at: %s", config->dir_path);

    // 分配WAL结构
    ppdb_wal_t* new_wal = (ppdb_wal_t*)malloc(sizeof(ppdb_wal_t));
    if (!new_wal) {
        ppdb_log_error("Failed to allocate WAL");
        return PPDB_ERR_NO_MEMORY;
    }

    // 初始化WAL结构
    strncpy(new_wal->dir_path, config->dir_path, MAX_PATH_LENGTH - 1);
    new_wal->dir_path[MAX_PATH_LENGTH - 1] = '\0';
    new_wal->segment_size = config->segment_size;
    new_wal->sync_write = config->sync_write;
    new_wal->current_fd = -1;
    new_wal->current_size = 0;

    // 初始化互斥锁
    if (pthread_mutex_init(&new_wal->mutex, NULL) != 0) {
        ppdb_log_error("Failed to initialize mutex");
        free(new_wal);
        return PPDB_ERR_MUTEX_ERROR;
    }

    // 创建WAL目录
    ppdb_error_t err = ppdb_ensure_directory(new_wal->dir_path);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create WAL directory: %s", new_wal->dir_path);
        pthread_mutex_destroy(&new_wal->mutex);
        free(new_wal);
        return err;
    }

    // 创建第一个WAL段
    err = create_new_segment(new_wal);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create initial WAL segment");
        pthread_mutex_destroy(&new_wal->mutex);
        free(new_wal);
        return err;
    }

    ppdb_log_info("Successfully created WAL at: %s", new_wal->dir_path);
    *wal = new_wal;
    return PPDB_OK;
}

// 销毁WAL实例
void ppdb_wal_destroy(ppdb_wal_t* wal) {
    if (!wal) return;

    ppdb_log_info("Destroying WAL at: %s", wal->dir_path);

    pthread_mutex_lock(&wal->mutex);
    if (wal->current_fd >= 0) {
        // 确保所有数据都写入磁盘
        fsync(wal->current_fd);
        close(wal->current_fd);
    }
    pthread_mutex_unlock(&wal->mutex);
    pthread_mutex_destroy(&wal->mutex);
    free(wal);
}

// 关闭WAL实例
void ppdb_wal_close(ppdb_wal_t* wal) {
    if (!wal) return;

    ppdb_log_info("Closing WAL at: %s", wal->dir_path);

    pthread_mutex_lock(&wal->mutex);
    if (wal->current_fd >= 0) {
        // 确保所有数据都写入磁盘
        if (wal->sync_write) {
            fsync(wal->current_fd);
        }
        close(wal->current_fd);
        wal->current_fd = -1;
    }
    pthread_mutex_unlock(&wal->mutex);
    pthread_mutex_destroy(&wal->mutex);
    free(wal);
}

// 创建新的WAL段文件
static ppdb_error_t create_new_segment(ppdb_wal_t* wal) {
    char filename[MAX_PATH_LENGTH];
    int written = snprintf(filename, sizeof(filename), "%s/%010zu.log",
                         wal->dir_path, wal->current_size);
    if (written < 0 || (size_t)written >= sizeof(filename)) {
        ppdb_log_error("Failed to create segment filename");
        return PPDB_ERR_IO;
    }

    ppdb_log_info("Creating new WAL segment: %s", filename);

    // 创建新文件,如果已存在则截断
    int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        ppdb_log_error("Failed to create WAL segment: %s", strerror(errno));
        return PPDB_ERR_IO;
    }

    // 写入WAL头部
    ppdb_wal_header_t header = {
        .magic = WAL_MAGIC,
        .version = WAL_VERSION,
        .segment_size = wal->segment_size,
        .reserved = 0
    };
    ssize_t written_bytes = write(fd, &header, sizeof(header));
    if (written_bytes != sizeof(header)) {
        ppdb_log_error("Failed to write WAL header: %s", strerror(errno));
        close(fd);
        return PPDB_ERR_IO;
    }

    // 同步写入
    if (wal->sync_write && fsync(fd) != 0) {
        ppdb_log_error("Failed to sync WAL header: %s", strerror(errno));
        close(fd);
        return PPDB_ERR_IO;
    }

    // 关闭旧文件
    if (wal->current_fd >= 0) {
        if (wal->sync_write) {
            fsync(wal->current_fd);
        }
        close(wal->current_fd);
    }

    wal->current_fd = fd;
    wal->current_size = sizeof(header);
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
    if (!wal || !key || key_size == 0 || key_size > MAX_KEY_SIZE ||
        (value_size > 0 && !value) || value_size > MAX_VALUE_SIZE) {
        ppdb_log_error("Invalid arguments: wal=%p, key=%p, key_size=%zu, value=%p, value_size=%zu",
                      wal, key, key_size, value, value_size);
        return PPDB_ERR_INVALID_ARG;
    }

    pthread_mutex_lock(&wal->mutex);

    // 计算记录大小
    size_t record_size = sizeof(ppdb_wal_record_header_t) + key_size + value_size;

    // 检查是否需要创建新的段
    if (wal->current_fd < 0 || wal->current_size + record_size > wal->segment_size) {
        ppdb_error_t err = create_new_segment(wal);
        if (err != PPDB_OK) {
            pthread_mutex_unlock(&wal->mutex);
            return err;
        }
    }

    // 写入记录头
    ppdb_wal_record_header_t record_header = {
        .type = type,
        .key_size = key_size,
        .value_size = value_size
    };
    ssize_t written = write(wal->current_fd, &record_header, sizeof(record_header));
    if (written != sizeof(record_header)) {
        ppdb_log_error("Failed to write record header: %s", strerror(errno));
        pthread_mutex_unlock(&wal->mutex);
        return PPDB_ERR_IO;
    }

    // 写入键
    written = write(wal->current_fd, key, key_size);
    if (written != key_size) {
        ppdb_log_error("Failed to write key: %s", strerror(errno));
        pthread_mutex_unlock(&wal->mutex);
        return PPDB_ERR_IO;
    }

    // 写入值(如果有)
    if (value_size > 0) {
        written = write(wal->current_fd, value, value_size);
        if (written != value_size) {
            ppdb_log_error("Failed to write value: %s", strerror(errno));
            pthread_mutex_unlock(&wal->mutex);
            return PPDB_ERR_IO;
        }
    }

    ppdb_log_info("Writing WAL record: type=%d, key_size=%zu, value_size=%zu",
                  type, key_size, value_size);

    // 同步写入
    if (wal->sync_write) {
        if (fsync(wal->current_fd) != 0) {
            ppdb_log_error("Failed to sync WAL to disk: %s", strerror(errno));
            pthread_mutex_unlock(&wal->mutex);
            return PPDB_ERR_IO;
        }
    }

    wal->current_size += record_size;
    pthread_mutex_unlock(&wal->mutex);
    return PPDB_OK;
}

// 比较WAL文件名(按数字序)
static int compare_wal_files(const void* a, const void* b) {
    const char* file_a = *(const char**)a;
    const char* file_b = *(const char**)b;
    
    // 提取文件名中的数字部分
    char* end_a;
    char* end_b;
    unsigned long num_a = strtoul(file_a, &end_a, 10);
    unsigned long num_b = strtoul(file_b, &end_b, 10);
    
    // 检查转换是否成功
    if (end_a == file_a && end_b == file_b) {
        return strcmp(file_a, file_b);
    }
    if (end_a == file_a) return 1;
    if (end_b == file_b) return -1;
    
    if (num_a < num_b) return -1;
    if (num_a > num_b) return 1;
    return 0;
}

// 恢复数据
ppdb_error_t ppdb_wal_recover(ppdb_wal_t* wal, ppdb_memtable_t* table) {
    if (!wal || !table) {
        ppdb_log_error("Invalid arguments: wal=%p, table=%p", wal, table);
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_log_info("Recovering WAL from: %s", wal->dir_path);

    // 对MemTable加锁
    pthread_mutex_lock(&wal->mutex);

    // 打开目录
    DIR* dir = opendir(wal->dir_path);
    if (!dir) {
        ppdb_log_error("Failed to open WAL directory: %s, error: %s", 
                      wal->dir_path, strerror(errno));
        pthread_mutex_unlock(&wal->mutex);
        return PPDB_ERR_IO;
    }

    // 收集所有WAL文件
    struct dirent* entry;
    char* wal_files[1024];  // 假设最多1024个WAL文件
    size_t file_count = 0;

    while ((entry = readdir(dir)) != NULL) {
        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 检查文件名是否以 .log 结尾
        size_t name_len = strlen(entry->d_name);
        if (name_len < 4 || strcmp(entry->d_name + name_len - 4, ".log") != 0) {
            continue;
        }

        // 保存文件名
        wal_files[file_count] = strdup(entry->d_name);
        if (!wal_files[file_count]) {
            ppdb_log_error("Failed to allocate memory for filename");
            for (size_t i = 0; i < file_count; i++) {
                free(wal_files[i]);
            }
            closedir(dir);
            pthread_mutex_unlock(&wal->mutex);
            return PPDB_ERR_NO_MEMORY;
        }
        file_count++;
    }
    closedir(dir);

    // 按照文件名数字大小排序
    qsort(wal_files, file_count, sizeof(char*), compare_wal_files);

    ppdb_error_t final_err = PPDB_OK;

    // 按顺序处理每个WAL文件
    for (size_t i = 0; i < file_count; i++) {
        // 构建完整路径
        char filename[MAX_PATH_LENGTH];
        snprintf(filename, sizeof(filename), "%s/%s", wal->dir_path, wal_files[i]);
        ppdb_log_info("Processing WAL file: %s", filename);

        // 打开WAL文件
        int fd = open(filename, O_RDONLY);
        if (fd < 0) {
            ppdb_log_error("Failed to open WAL file: %s, error: %s", 
                          filename, strerror(errno));
            final_err = PPDB_ERR_IO;
            break;
        }

        // 读取WAL头部
        ppdb_wal_header_t header;
        ssize_t read_size = read(fd, &header, sizeof(header));
        if (read_size != sizeof(header)) {
            ppdb_log_error("Failed to read WAL header: %s, error: %s", 
                          filename, strerror(errno));
            close(fd);
            final_err = PPDB_ERR_IO;
            break;
        }

        // 验证WAL头部
        if (header.magic != WAL_MAGIC || header.version != WAL_VERSION) {
            ppdb_log_error("Invalid WAL header: magic=0x%x, version=%d",
                          header.magic, header.version);
            close(fd);
            final_err = PPDB_ERR_CORRUPTED;
            break;
        }
        ppdb_log_info("WAL header valid: magic=0x%x, version=%d, segment_size=%d",
                     header.magic, header.version, header.segment_size);

        // 读取并应用记录
        size_t total_records = 0;
        while (1) {
            // 读取记录头
            ppdb_wal_record_header_t record_header;
            read_size = read(fd, &record_header, sizeof(record_header));
            if (read_size == 0) {
                // 文件结束
                break;
            }
            if (read_size != sizeof(record_header)) {
                if (read_size < 0) {
                    ppdb_log_error("Error reading record header: %s, error: %s", 
                                filename, strerror(errno));
                    final_err = PPDB_ERR_IO;
                } else {
                    ppdb_log_warn("Incomplete record header in file: %s", filename);
                }
                break;
            }

            // 验证记录头
            if (record_header.key_size > MAX_KEY_SIZE ||
                record_header.value_size > MAX_VALUE_SIZE) {
                ppdb_log_error("Invalid record size in file %s: key_size=%d, value_size=%d",
                              filename, record_header.key_size, record_header.value_size);
                final_err = PPDB_ERR_CORRUPTED;
                break;
            }

            // 读取键
            uint8_t* key = (uint8_t*)malloc(record_header.key_size);
            if (!key) {
                ppdb_log_error("Failed to allocate memory for key");
                final_err = PPDB_ERR_NO_MEMORY;
                break;
            }
            read_size = read(fd, key, record_header.key_size);
            if (read_size != record_header.key_size) {
                ppdb_log_error("Failed to read key from file %s: expected %d bytes, got %zd bytes",
                              filename, record_header.key_size, read_size);
                free(key);
                final_err = PPDB_ERR_CORRUPTED;
                break;
            }

            // 读取值
            uint8_t* value = NULL;
            if (record_header.value_size > 0) {
                value = (uint8_t*)malloc(record_header.value_size);
                if (!value) {
                    ppdb_log_error("Failed to allocate memory for value");
                    free(key);
                    final_err = PPDB_ERR_NO_MEMORY;
                    break;
                }
                read_size = read(fd, value, record_header.value_size);
                if (read_size != record_header.value_size) {
                    ppdb_log_error("Failed to read value from file %s: expected %d bytes, got %zd bytes",
                                  filename, record_header.value_size, read_size);
                    free(key);
                    free(value);
                    final_err = PPDB_ERR_CORRUPTED;
                    break;
                }
            }

            // 应用记录
            ppdb_error_t err;
            switch (record_header.type) {
            case PPDB_WAL_RECORD_PUT:
                err = ppdb_memtable_put(table, key, record_header.key_size,
                                      value, record_header.value_size);
                if (err != PPDB_OK && err != PPDB_ERR_FULL) {
                    ppdb_log_error("Failed to apply PUT record in file %s: %d", filename, err);
                    final_err = err;
                }
                break;
            case PPDB_WAL_RECORD_DELETE:
                err = ppdb_memtable_delete(table, key, record_header.key_size);
                if (err != PPDB_OK && err != PPDB_ERR_NOT_FOUND) {
                    ppdb_log_error("Failed to apply DELETE record in file %s: %d", filename, err);
                    final_err = err;
                }
                break;
            default:
                ppdb_log_error("Invalid record type in file %s: %d", 
                              filename, record_header.type);
                final_err = PPDB_ERR_CORRUPTED;
                break;
            }

            // 释放内存
            free(key);
            if (value) {
                free(value);
            }

            if (final_err != PPDB_OK) {
                break;
            }

            total_records++;
        }

        close(fd);
        ppdb_log_info("Processed %zu records from WAL file: %s", total_records, filename);

        if (final_err != PPDB_OK) {
            break;
        }
    }

    // 释放文件名内存
    for (size_t i = 0; i < file_count; i++) {
        free(wal_files[i]);
    }

    if (final_err != PPDB_OK) {
        ppdb_log_error("WAL recovery failed with error: %d", final_err);
    } else {
        ppdb_log_info("WAL recovery completed successfully");
    }

    pthread_mutex_unlock(&wal->mutex);
    return final_err;
}

// 归档旧的WAL文件
static ppdb_error_t archive_old_wal_files(const char* wal_dir) {
    if (!wal_dir) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 创建归档目录
    char archive_dir[MAX_PATH_LENGTH];
    snprintf(archive_dir, sizeof(archive_dir), "%s/archive", wal_dir);
    ppdb_error_t err = ppdb_ensure_directory(archive_dir);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create archive directory: %s", archive_dir);
        return err;
    }

    // 打开WAL目录
    DIR* dir = opendir(wal_dir);
    if (!dir) {
        ppdb_log_error("Failed to open WAL directory: %s", strerror(errno));
        return PPDB_ERR_IO;
    }

    // 遍历所有WAL文件
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // 检查文件名是否以 .log 结尾
        size_t name_len = strlen(entry->d_name);
        if (name_len < 4 || strcmp(entry->d_name + name_len - 4, ".log") != 0) {
            continue;
        }

        // 构建源文件和目标文件路径
        char src_path[MAX_PATH_LENGTH];
        char dst_path[MAX_PATH_LENGTH];
        snprintf(src_path, sizeof(src_path), "%s/%s", wal_dir, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", archive_dir, entry->d_name);

        // 移动文件
        if (rename(src_path, dst_path) != 0) {
            ppdb_log_error("Failed to move WAL file: %s -> %s, error: %s",
                          src_path, dst_path, strerror(errno));
            continue;
        }
        ppdb_log_info("Archived WAL file: %s -> %s", src_path, dst_path);
    }

    closedir(dir);
    return PPDB_OK;
}

// 归档WAL文件
ppdb_error_t ppdb_wal_archive(ppdb_wal_t* wal) {
    if (!wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    pthread_mutex_lock(&wal->mutex);
    ppdb_error_t err = archive_old_wal_files(wal->dir_path);
    pthread_mutex_unlock(&wal->mutex);
    return err;
}