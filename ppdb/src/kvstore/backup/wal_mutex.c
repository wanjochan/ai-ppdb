#include <cosmopolitan.h>
#include "ppdb/wal_mutex.h"
#include "ppdb/memtable_mutex.h"
#include "ppdb/error.h"
#include "ppdb/defs.h"
#include "ppdb/memtable.h"
#include "ppdb/logger.h"
#include "ppdb/fs.h"

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
    int current_fd;                  // 当前文件描述符
    size_t current_size;             // 当前段大小
    size_t segment_id;               // 当前段ID
    pthread_mutex_t mutex;           // 互斥锁
} ppdb_wal_t;

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

    // Allocate and zero initialize the WAL structure
    ppdb_wal_t* new_wal = (ppdb_wal_t*)calloc(1, sizeof(ppdb_wal_t));
    if (!new_wal) {
        ppdb_log_error("Failed to allocate WAL");
        return PPDB_ERR_NO_MEMORY;
    }

    // Initialize basic fields
    size_t path_len = strlen(config->dir_path);
    if (path_len >= sizeof(new_wal->dir_path)) {
        ppdb_log_error("Directory path too long");
        free(new_wal);
        return PPDB_ERR_PATH_TOO_LONG;
    }
    memcpy(new_wal->dir_path, config->dir_path, path_len);
    new_wal->dir_path[path_len] = '\0';

    new_wal->segment_size = config->segment_size;
    new_wal->sync_write = config->sync_write;
    new_wal->current_fd = -1;
    new_wal->current_size = 0;
    new_wal->segment_id = 0;

    // Initialize mutex
    if (pthread_mutex_init(&new_wal->mutex, NULL) != 0) {
        ppdb_log_error("Failed to initialize mutex");
        free(new_wal);
        return PPDB_ERR_MUTEX_ERROR;
    }

    // Create WAL directory
    if (!ppdb_fs_dir_exists(config->dir_path)) {
        ppdb_error_t err = ppdb_ensure_directory(config->dir_path);
        if (err != PPDB_OK) {
            ppdb_log_error("Failed to create WAL directory: %s", config->dir_path);
            pthread_mutex_destroy(&new_wal->mutex);
            free(new_wal);
            return err;
        }
    }

    // Wait for directory creation to complete
    usleep(50000);  // 50ms

    // Create new WAL segment
    ppdb_error_t err = create_new_segment(new_wal);
    if (err != PPDB_OK) {
        ppdb_log_error("Failed to create new WAL segment: %s", ppdb_error_string(err));
        pthread_mutex_destroy(&new_wal->mutex);
        free(new_wal);
        return err;
    }

    // Wait for segment creation to complete
    usleep(50000);  // 50ms

    *wal = new_wal;
    ppdb_log_info("WAL created successfully");
    return PPDB_OK;
}

// 销毁WAL实例
void ppdb_wal_destroy(ppdb_wal_t* wal) {
    if (!wal) return;

    // 先加锁
    pthread_mutex_lock(&wal->mutex);

    // 关闭当前文件
    if (wal->current_fd >= 0) {
        if (wal->sync_write) {
            fsync(wal->current_fd);
            // 等待同步完成
            usleep(50000);  // 50ms
        }
        close(wal->current_fd);
        wal->current_fd = -1;
        // 等待关闭完成
        usleep(50000);  // 50ms
    }

    // 解锁并销毁互斥锁
    pthread_mutex_unlock(&wal->mutex);
    pthread_mutex_destroy(&wal->mutex);

    // 清零内存并释放
    memset(wal, 0, sizeof(ppdb_wal_t));
    free(wal);
}

// 关闭WAL实例
void ppdb_wal_close(ppdb_wal_t* wal) {
    if (!wal) return;

    // Lock the WAL
    pthread_mutex_lock(&wal->mutex);

    // Close current file descriptor
    if (wal->current_fd >= 0) {
        if (wal->sync_write) {
            fsync(wal->current_fd);
            // Wait for sync to complete
            usleep(50000);  // 50ms
        }
        close(wal->current_fd);
        wal->current_fd = -1;
        // Wait for close to complete
        usleep(50000);  // 50ms
    }

    // Reset fields
    wal->current_size = 0;
    wal->segment_id = 0;

    // Unlock and destroy mutex
    pthread_mutex_unlock(&wal->mutex);
    pthread_mutex_destroy(&wal->mutex);

    // Wait for mutex destruction to complete
    usleep(50000);  // 50ms

    // Zero out and free WAL structure
    memset(wal, 0, sizeof(ppdb_wal_t));
    free(wal);
}

// 创建新的WAL段文件
static ppdb_error_t create_new_segment(ppdb_wal_t* wal) {
    if (!wal) {
        ppdb_log_error("Invalid argument: wal is NULL");
        return PPDB_ERR_NULL_POINTER;
    }

    // Close current file if open
    if (wal->current_fd >= 0) {
        if (wal->sync_write) {
            fsync(wal->current_fd);
        }
        close(wal->current_fd);
        wal->current_fd = -1;
        
        // Wait for file to be closed
        usleep(50000);  // 50ms
    }

    // Generate new segment file name
    char segment_path[MAX_PATH_LENGTH];
    size_t dir_len = strlen(wal->dir_path);
    
    // Safety check: ensure path length won't overflow
    if (dir_len > MAX_PATH_LENGTH - 20) {  // 20 = len("/%09zu.log") + 1
        ppdb_log_error("WAL directory path too long");
        return PPDB_ERR_PATH_TOO_LONG;
    }
    
    ssize_t path_len = snprintf(segment_path, sizeof(segment_path), WAL_SEGMENT_NAME_FMT,
                               wal->dir_path, wal->segment_id);
    
    if (path_len < 0 || (size_t)path_len >= sizeof(segment_path)) {
        ppdb_log_error("Failed to construct WAL segment path");
        return PPDB_ERR_PATH_TOO_LONG;
    }

    // Check segment ID limit
    if (wal->segment_id >= WAL_SEGMENT_ID_MAX) {
        ppdb_log_error("WAL segment ID overflow: %zu", wal->segment_id);
        return PPDB_ERR_LIMIT_EXCEEDED;
    }

    // Create new segment file
    int fd = open(segment_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        ppdb_log_error("Failed to create WAL segment: %s (%s)", segment_path, strerror(errno));
        return PPDB_ERR_IO;
    }

    // Write WAL header
    ppdb_wal_header_t header = {
        .magic = WAL_MAGIC,
        .version = WAL_VERSION,
        .segment_size = (uint32_t)wal->segment_size,
        .reserved = 0
    };

    ssize_t bytes_written = write(fd, &header, sizeof(header));
    if (bytes_written != sizeof(header)) {
        ppdb_log_error("Failed to write WAL header: %s", strerror(errno));
        close(fd);
        return PPDB_ERR_IO;
    }

    // Sync if needed
    if (wal->sync_write) {
        if (fsync(fd) != 0) {
            ppdb_log_error("Failed to sync WAL header: %s", strerror(errno));
            close(fd);
            return PPDB_ERR_IO;
        }
        // Wait for sync to complete
        usleep(50000);  // 50ms
    }

    // Update WAL state
    wal->current_fd = fd;
    wal->current_size = sizeof(header);
    wal->segment_id++;

    ppdb_log_debug("Created new WAL segment: %s", segment_path);
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
    if (written < 0 || (size_t)written != sizeof(record_header)) {
        ppdb_log_error("Failed to write record header: %s", strerror(errno));
        pthread_mutex_unlock(&wal->mutex);
        return PPDB_ERR_IO;
    }

    // 写入键
    written = write(wal->current_fd, key, key_size);
    if (written < 0 || (size_t)written != key_size) {
        ppdb_log_error("Failed to write key: %s", strerror(errno));
        pthread_mutex_unlock(&wal->mutex);
        return PPDB_ERR_IO;
    }

    // 写入值(如果有)
    if (value_size > 0) {
        written = write(wal->current_fd, value, value_size);
        if (written < 0 || (size_t)written != value_size) {
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

// 从WAL恢复数据
ppdb_error_t ppdb_wal_recover(ppdb_wal_t* wal, ppdb_memtable_t** table) {
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

    // 收集所有WAL文件
    struct dirent* entry;
    char** wal_files = NULL;
    size_t num_files = 0;
    size_t capacity = 16;
    int current_fd = -1;  // 添加文件描述符变量

    wal_files = (char**)malloc(capacity * sizeof(char*));
    if (!wal_files) {
        closedir(dir);
        return PPDB_ERR_NO_MEMORY;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, ".log") != NULL) {
            if (num_files >= capacity) {
                capacity *= 2;
                char** new_files = (char**)realloc(wal_files, capacity * sizeof(char*));
                if (!new_files) {
                    for (size_t i = 0; i < num_files; i++) {
                        free(wal_files[i]);
                    }
                    free(wal_files);
                    closedir(dir);
                    return PPDB_ERR_NO_MEMORY;
                }
                wal_files = new_files;
            }

            wal_files[num_files] = strdup(entry->d_name);
            if (!wal_files[num_files]) {
                for (size_t i = 0; i < num_files; i++) {
                    free(wal_files[i]);
                }
                free(wal_files);
                closedir(dir);
                return PPDB_ERR_NO_MEMORY;
            }
            num_files++;
        }
    }

    closedir(dir);

    if (num_files == 0) {
        free(wal_files);
        ppdb_log_info("No WAL files found");
        return PPDB_OK;
    }

    // 按文件名排序(文件名是按时间顺序编号的)
    qsort(wal_files, num_files, sizeof(char*), compare_wal_files);

    // 按顺序处理每个WAL文件
    ppdb_error_t err = PPDB_OK;
    for (size_t i = 0; i < num_files && err == PPDB_OK; i++) {
        char path[MAX_PATH_LENGTH];
        int written = snprintf(path, sizeof(path), "%s/%s", wal->dir_path, wal_files[i]);
        if (written < 0 || written >= (int)sizeof(path)) {
            ppdb_log_error("Path too long: %s/%s", wal->dir_path, wal_files[i]);
            continue;
        }

        ppdb_log_info("Processing WAL file: %s", path);

        current_fd = open(path, O_RDONLY);
        if (current_fd == -1) {
            ppdb_log_error("Failed to open WAL file: %s", strerror(errno));
            err = PPDB_ERR_IO;
            goto cleanup;
        }

        // 读取并验证WAL头部
        ppdb_wal_header_t header;
        ssize_t read_bytes = read(current_fd, &header, sizeof(header));
        if (read_bytes != sizeof(header)) {
            ppdb_log_error("Failed to read WAL header: %s", strerror(errno));
            err = PPDB_ERR_IO;
            goto cleanup;
        }

        if (header.magic != WAL_MAGIC) {
            ppdb_log_error("Invalid WAL magic number");
            err = PPDB_ERR_CORRUPTED;
            goto cleanup;
        }

        ppdb_log_info("WAL header valid: magic=0x%x, version=%d, segment_size=%d",
                     header.magic, header.version, header.segment_size);

        // 读取并应用记录
        size_t processed_records = 0;
        while (1) {
            ppdb_wal_record_header_t record_header;
            read_bytes = read(current_fd, &record_header, sizeof(record_header));
            if (read_bytes == 0) {
                break;  // 文件结束
            }
            if (read_bytes != sizeof(record_header)) {
                if (read_bytes == -1) {
                    ppdb_log_error("Failed to read record header: %s", strerror(errno));
                    err = PPDB_ERR_IO;
                    goto cleanup;
                }
                // 如果读取的字节数不完整，说明文件已经结束
                ppdb_log_warn("Incomplete record header at end of file");
                break;
            }

            // 验证记录头部
            if (record_header.key_size == 0 || record_header.key_size > MAX_KEY_SIZE ||
                record_header.value_size > MAX_VALUE_SIZE) {
                ppdb_log_error("Invalid record header: key_size=%u, value_size=%u",
                             record_header.key_size, record_header.value_size);
                err = PPDB_ERR_CORRUPTED;
                goto cleanup;
            }

            // 读取键
            uint8_t* key = malloc(record_header.key_size);
            uint8_t* value = NULL;
            if (!key) {
                ppdb_log_error("Failed to allocate key buffer");
                err = PPDB_ERR_NO_MEMORY;
                goto cleanup;
            }

            read_bytes = read(current_fd, key, record_header.key_size);
            if (read_bytes != record_header.key_size) {
                ppdb_log_error("Failed to read key: %s", strerror(errno));
                free(key);
                err = PPDB_ERR_IO;
                goto cleanup;
            }

            // 对于PUT操作，读取并应用值
            if (record_header.type == PPDB_WAL_RECORD_PUT) {
                if (record_header.value_size > 0) {
                    value = malloc(record_header.value_size);
                    if (!value) {
                        ppdb_log_error("Failed to allocate value buffer");
                        free(key);
                        err = PPDB_ERR_NO_MEMORY;
                        goto cleanup;
                    }

                    read_bytes = read(current_fd, value, record_header.value_size);
                    if (read_bytes != record_header.value_size) {
                        ppdb_log_error("Failed to read value: %s", strerror(errno));
                        free(key);
                        free(value);
                        err = PPDB_ERR_IO;
                        goto cleanup;
                    }
                }

                err = ppdb_memtable_put(*table, key, record_header.key_size,
                                      value, record_header.value_size);
            } else {
                err = ppdb_memtable_delete(*table, key, record_header.key_size);
            }

            free(key);
            if (value) {
                free(value);
                value = NULL;
            }

            if (err != PPDB_OK && err != PPDB_ERR_NOT_FOUND) {
                ppdb_log_error("Failed to apply record: %s", ppdb_error_string(err));
                goto cleanup;
            }

            processed_records++;
        }

        ppdb_log_info("Processed %zu records from WAL file: %s", processed_records, path);
        close(current_fd);
        current_fd = -1;
    }

cleanup:
    // 清理资源
    if (current_fd >= 0) {
        close(current_fd);
    }

    for (size_t i = 0; i < num_files; i++) {
        free(wal_files[i]);
    }
    free(wal_files);

    if (err == PPDB_OK) {
        ppdb_log_info("WAL recovery completed successfully");
    } else {
        ppdb_log_error("WAL recovery failed with error: %s", ppdb_error_string(err));
    }
    return err;
}

// 归档旧的WAL文件
static ppdb_error_t archive_old_wal_files(const char* wal_dir) {
    if (!wal_dir) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 创建归档目录
    char archive_dir[MAX_PATH_LENGTH];
    int written = snprintf(archive_dir, sizeof(archive_dir), "%s/archive", wal_dir);
    if (written < 0 || written >= (int)sizeof(archive_dir)) {
        ppdb_log_error("Failed to create archive directory path");
        return PPDB_ERR_IO;
    }

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

        char src_path[MAX_PATH_LENGTH];
        char dst_path[MAX_PATH_LENGTH];
        
        int written = snprintf(src_path, sizeof(src_path), "%s/%s", wal_dir, entry->d_name);
        if (written < 0 || written >= (int)sizeof(src_path)) {
            ppdb_log_error("Source path too long: %s/%s", wal_dir, entry->d_name);
            continue;
        }

        written = snprintf(dst_path, sizeof(dst_path), "%s/%s", archive_dir, entry->d_name);
        if (written < 0 || written >= (int)sizeof(dst_path)) {
            ppdb_log_error("Destination path too long: %s/%s", archive_dir, entry->d_name);
            continue;
        }

        // 构建源文件和目标文件路径
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