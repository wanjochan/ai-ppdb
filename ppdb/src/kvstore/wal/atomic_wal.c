#include <cosmopolitan.h>
#include "ppdb/wal.h"
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

// 创建WAL实例
ppdb_error_t ppdb_wal_create_lockfree(const ppdb_wal_config_t* config, ppdb_wal_t** wal) {
    if (!config || !wal || !config->dir_path || config->dir_path[0] == '\0') {
        ppdb_log_error("Invalid arguments: config=%p, wal=%p", config, wal);
        return PPDB_ERR_INVALID_ARG;
    }

    // 检查目录路径长度
    size_t dir_path_len = strlen(config->dir_path);
    if (dir_path_len > WAL_PATH_LENGTH - 20) {  // 预留足够空间给文件名
        ppdb_log_error("Directory path too long: %s", config->dir_path);
        return PPDB_ERR_PATH_TOO_LONG;
    }

    ppdb_log_info("Creating WAL at: %s", config->dir_path);

    ppdb_wal_t* new_wal = (ppdb_wal_t*)malloc(sizeof(ppdb_wal_t));
    if (!new_wal) {
        ppdb_log_error("Failed to allocate WAL");
        return PPDB_ERR_NO_MEMORY;
    }

    // 构造WAL文件路径
    char wal_path[WAL_PATH_LENGTH];
    int n = snprintf(wal_path, sizeof(wal_path), "%s/wal", config->dir_path);
    if (n < 0 || (size_t)n >= sizeof(wal_path)) {
        ppdb_log_error("WAL path too long: %s/wal", config->dir_path);
        free(new_wal);
        return PPDB_ERR_PATH_TOO_LONG;
    }

    strncpy(new_wal->dir_path, config->dir_path, MAX_PATH_LENGTH - 1);
    new_wal->dir_path[MAX_PATH_LENGTH - 1] = '\0';
    new_wal->segment_size = config->segment_size;
    new_wal->sync_write = config->sync_write;
    atomic_init(&new_wal->current_fd, -1);
    atomic_init(&new_wal->current_size, 0);
    atomic_init(&new_wal->segment_id, 0);

    ppdb_error_t err = ppdb_ensure_directory(new_wal->dir_path);
    if (err != PPDB_OK && err != PPDB_ERR_EXISTS) {
        ppdb_log_error("Failed to create WAL directory: %s", new_wal->dir_path);
        free(new_wal);
        return err;
    }

    // 获取目录中最大的段ID
    DIR* dir = opendir(new_wal->dir_path);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG && strstr(entry->d_name, ".log") != NULL) {
                char* end;
                unsigned long id = strtoul(entry->d_name, &end, 10);
                if (end != entry->d_name && id > atomic_load(&new_wal->segment_id)) {
                    atomic_store(&new_wal->segment_id, id);
                }
            }
        }
        closedir(dir);
    }

    char filename[WAL_PATH_LENGTH];
    n = snprintf(filename, sizeof(filename), "%s/%010zu.log",
             new_wal->dir_path, atomic_load(&new_wal->segment_id));
    if (n < 0 || (size_t)n >= sizeof(filename)) {
        ppdb_log_error("WAL filename too long");
        free(new_wal);
        return PPDB_ERR_PATH_TOO_LONG;
    }

    int fd = open(filename, O_RDWR | O_CREAT, 0644);
    if (fd == -1) {
        ppdb_log_error("Failed to create WAL segment: %s", strerror(errno));
        free(new_wal);
        return PPDB_ERR_IO;
    }

    ppdb_wal_header_t header = {
        .magic = WAL_MAGIC,
        .version = WAL_VERSION,
        .segment_size = new_wal->segment_size,
        .reserved = 0
    };

    ssize_t written = write(fd, &header, sizeof(header));
    if (written != sizeof(header)) {
        ppdb_log_error("Failed to write WAL header: %s", strerror(errno));
        close(fd);
        free(new_wal);
        return PPDB_ERR_IO;
    }

    atomic_store(&new_wal->current_fd, fd);
    atomic_store(&new_wal->current_size, sizeof(header));

    ppdb_log_info("Successfully created WAL at: %s", new_wal->dir_path);
    *wal = new_wal;
    return PPDB_OK;
}

void ppdb_wal_destroy_lockfree(ppdb_wal_t* wal) {
    if (!wal) return;

    ppdb_log_info("Destroying WAL at: %s", wal->dir_path);

    int fd = atomic_load(&wal->current_fd);
    if (fd >= 0) {
        fsync(fd);
        close(fd);
    }

    DIR* dir = opendir(wal->dir_path);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
            
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
        rmdir(wal->dir_path);
    }

    free(wal);
}

void ppdb_wal_close_lockfree(ppdb_wal_t* wal) {
    if (!wal) return;

    ppdb_log_info("Closing WAL at: %s", wal->dir_path);

    int fd = atomic_load(&wal->current_fd);
    if (fd >= 0) {
        fsync(fd);
        close(fd);
    }
    
    free(wal);
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
    if (!wal || !table) return PPDB_ERR_INVALID_ARG;

    // 创建新的MemTable
    ppdb_error_t err = ppdb_memtable_create_lockfree(wal->segment_size, table);
    if (err != PPDB_OK) return err;

    DIR* dir = opendir(wal->dir_path);
    if (!dir) return PPDB_OK;  // 没有WAL文件，返回空表

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type != DT_REG || !strstr(entry->d_name, ".log")) continue;

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

        int fd = open(path, O_RDONLY);
        if (fd == -1) continue;

        ppdb_wal_header_t header;
        ssize_t read_size = read(fd, &header, sizeof(header));
        if (read_size < 0 || (size_t)read_size != sizeof(header) || header.magic != WAL_MAGIC) {
            close(fd);
            continue;
        }

        while (1) {
            ppdb_wal_record_header_t record;
            if (read(fd, &record, sizeof(record)) != sizeof(record)) break;

            uint8_t* key = malloc(record.key_size);
            uint8_t* value = record.type == PPDB_WAL_RECORD_PUT ? malloc(record.value_size) : NULL;

            if (!key || (record.type == PPDB_WAL_RECORD_PUT && !value)) {
                free(key);
                free(value);
                close(fd);
                ppdb_memtable_destroy_lockfree(*table);
                *table = NULL;
                closedir(dir);
                return PPDB_ERR_NO_MEMORY;
            }

            if (read(fd, key, record.key_size) != record.key_size ||
                (record.type == PPDB_WAL_RECORD_PUT && read(fd, value, record.value_size) != record.value_size)) {
                free(key);
                free(value);
                break;
            }

            if (record.type == PPDB_WAL_RECORD_PUT) {
                err = ppdb_memtable_put_lockfree(*table, key, record.key_size, value, record.value_size);
            } else {
                err = ppdb_memtable_delete_lockfree(*table, key, record.key_size);
            }

            free(key);
            free(value);

            if (err != PPDB_OK && err != PPDB_ERR_NOT_FOUND) {
                close(fd);
                ppdb_memtable_destroy_lockfree(*table);
                *table = NULL;
                closedir(dir);
                return err;
            }
        }

        close(fd);
    }

    closedir(dir);
    return PPDB_OK;
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