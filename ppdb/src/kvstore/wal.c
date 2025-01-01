#include <cosmopolitan.h>
#include "ppdb/ppdb_wal.h"
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_fs.h"
#include "ppdb/ppdb_logger.h"
#include "kvstore/internal/kvstore_memtable.h"
#include "kvstore/internal/kvstore_wal.h"
#include "kvstore/internal/kvstore_wal_types.h"
#include "kvstore/internal/kvstore_fs.h"
#include "kvstore/internal/sync.h"
#include "kvstore/internal/metrics.h"

// 内部函数声明
static ppdb_error_t scan_existing_segments(ppdb_wal_t* wal);
static ppdb_error_t create_new_segment(ppdb_wal_t* wal, wal_segment_t** segment);
static ppdb_error_t seal_segment(wal_segment_t* segment);
static ppdb_error_t cleanup_old_segments(ppdb_wal_t* wal);
static ppdb_error_t write_record_to_segment(wal_segment_t* segment, const void* key, size_t key_size,
                                          const void* value, size_t value_size, uint64_t sequence);

// CRC32计算函数
uint32_t calculate_crc32(const void* data, size_t size) {
    if (!data || size == 0) return 0;
    return crc32c(0, data, size);
}

// 生成段文件名
char* generate_segment_filename(const char* dir_path, uint64_t segment_id) {
    char* filename = malloc(strlen(dir_path) + 32);
    if (!filename) return NULL;
    snprintf(filename, strlen(dir_path) + 32, "%s/wal-%06" PRIu64 ".log", dir_path, segment_id);
    return filename;
}

// 验证段的完整性
ppdb_error_t validate_segment(wal_segment_t* segment) {
    if (!segment || segment->fd < 0) return PPDB_ERR_INVALID_ARG;

    // 读取段头部
    wal_segment_header_t header;
    ssize_t read_size = pread(segment->fd, &header, WAL_SEGMENT_HEADER_SIZE, 0);
    if (read_size != WAL_SEGMENT_HEADER_SIZE) return PPDB_ERR_IO;

    // 验证魔数和版本
    if (header.magic != WAL_MAGIC || header.version != WAL_VERSION) {
        return PPDB_ERR_WAL_CORRUPTED;
    }

    // 验证头部校验和
    uint32_t saved_checksum = header.checksum;
    header.checksum = 0;
    uint32_t computed_checksum = calculate_crc32(&header, WAL_SEGMENT_HEADER_SIZE);
    if (computed_checksum != saved_checksum) {
        return PPDB_ERR_WAL_CORRUPTED;
    }

    // 更新段信息
    segment->first_sequence = header.first_sequence;
    segment->last_sequence = header.last_sequence;
    
    return PPDB_OK;
}

// 扫描现有段
ppdb_error_t scan_existing_segments(ppdb_wal_t* wal) {
    DIR* dir = opendir(wal->dir_path);
    if (!dir) return PPDB_ERR_IO;

    struct dirent* entry;
    uint64_t max_segment_id = 0;
    
    // 第一遍：收集所有段ID
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "wal-", 4) == 0 && 
            strstr(entry->d_name, ".log") != NULL) {
            uint64_t segment_id;
            if (sscanf(entry->d_name, "wal-%06" PRIu64 ".log", &segment_id) == 1) {
                max_segment_id = max_segment_id > segment_id ? max_segment_id : segment_id;
            }
        }
    }

    // 重置目录流
    rewinddir(dir);
    
    // 第二遍：按顺序加载段
    for (uint64_t id = 0; id <= max_segment_id; id++) {
        char filename[256];
        snprintf(filename, sizeof(filename), "wal-%06" PRIu64 ".log", id);
        
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, filename) == 0) {
                wal_segment_t* segment = malloc(sizeof(wal_segment_t));
                if (!segment) {
                    closedir(dir);
                    return PPDB_ERR_OUT_OF_MEMORY;
                }

                segment->id = id;
                segment->filename = generate_segment_filename(wal->dir_path, id);
                if (!segment->filename) {
                    free(segment);
                    closedir(dir);
                    return PPDB_ERR_OUT_OF_MEMORY;
                }

                segment->fd = open(segment->filename, O_RDWR, 0644);
                if (segment->fd < 0) {
                    free(segment->filename);
                    free(segment);
                    continue;  // 跳过无法打开的段
                }

                // 获取段大小
                segment->size = lseek(segment->fd, 0, SEEK_END);
                if (segment->size < 0) {
                    close(segment->fd);
                    free(segment->filename);
                    free(segment);
                    continue;
                }

                // 验证段完整性
                if (validate_segment(segment) != PPDB_OK) {
                    close(segment->fd);
                    free(segment->filename);
                    free(segment);
                    continue;
                }

                segment->next = NULL;
                segment->is_sealed = true;  // 已存在的段都视为已封存

                // 添加到链表
                if (!wal->segments) {
                    wal->segments = segment;
                } else {
                    wal_segment_t* last = wal->segments;
                    while (last->next) {
                        last = last->next;
                    }
                    last->next = segment;
                }

                wal->segment_count++;
                break;
            }
        }
    }

    closedir(dir);
    wal->next_segment_id = max_segment_id + 1;
    return PPDB_OK;
}

// 创建新的 WAL 段
ppdb_error_t create_new_segment(ppdb_wal_t* wal, wal_segment_t** segment) {
    wal_segment_t* new_segment = malloc(sizeof(wal_segment_t));
    if (!new_segment) return PPDB_ERR_OUT_OF_MEMORY;

    new_segment->id = wal->next_segment_id++;
    new_segment->filename = generate_segment_filename(wal->dir_path, new_segment->id);
    if (!new_segment->filename) {
        free(new_segment);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 创建并打开段文件
    new_segment->fd = open(new_segment->filename, O_CREAT | O_RDWR, 0644);
    if (new_segment->fd < 0) {
        free(new_segment->filename);
        free(new_segment);
        return PPDB_ERR_IO;
    }

    // 写入段头部
    wal_segment_header_t header = {
        .magic = WAL_MAGIC,
        .version = WAL_VERSION,
        .first_sequence = wal->next_sequence,
        .last_sequence = wal->next_sequence - 1,  // 还没有记录
        .record_count = 0,
        .checksum = 0
    };

    // 计算校验和
    header.checksum = calculate_crc32(&header, WAL_SEGMENT_HEADER_SIZE);

    if (write(new_segment->fd, &header, WAL_SEGMENT_HEADER_SIZE) != WAL_SEGMENT_HEADER_SIZE) {
        close(new_segment->fd);
        unlink(new_segment->filename);
        free(new_segment->filename);
        free(new_segment);
        return PPDB_ERR_IO;
    }

    new_segment->size = WAL_SEGMENT_HEADER_SIZE;
    new_segment->next = NULL;
    new_segment->is_sealed = false;
    new_segment->first_sequence = header.first_sequence;
    new_segment->last_sequence = header.last_sequence;

    // 将新段添加到链表末尾
    if (!wal->segments) {
        wal->segments = new_segment;
    } else {
        wal_segment_t* last = wal->segments;
        while (last->next) {
            last = last->next;
        }
        last->next = new_segment;
    }

    wal->segment_count++;
    *segment = new_segment;
    return PPDB_OK;
}

// 封存段
ppdb_error_t seal_segment(wal_segment_t* segment) {
    if (!segment || segment->is_sealed) return PPDB_OK;
    
    // 更新段头部
    wal_segment_header_t header;
    if (pread(segment->fd, &header, WAL_SEGMENT_HEADER_SIZE, 0) != WAL_SEGMENT_HEADER_SIZE) {
        return PPDB_ERR_IO;
    }

    header.last_sequence = segment->last_sequence;
    header.checksum = 0;
    header.checksum = calculate_crc32(&header, WAL_SEGMENT_HEADER_SIZE);

    if (pwrite(segment->fd, &header, WAL_SEGMENT_HEADER_SIZE, 0) != WAL_SEGMENT_HEADER_SIZE) {
        return PPDB_ERR_IO;
    }

    // 同步文件
    if (fsync(segment->fd) != 0) {
        return PPDB_ERR_IO;
    }

    segment->is_sealed = true;
    return PPDB_OK;
}

// 清理旧段
ppdb_error_t cleanup_old_segments(ppdb_wal_t* wal) {
    if (!wal || wal->segment_count <= wal->config.max_segments) {
        return PPDB_OK;
    }

    size_t segments_to_remove = wal->segment_count - wal->config.max_segments;
    wal_segment_t* curr = wal->segments;

    for (size_t i = 0; i < segments_to_remove && curr; i++) {
        wal_segment_t* to_remove = curr;
        curr = curr->next;

        // 关闭并删除文件
        close(to_remove->fd);
        unlink(to_remove->filename);
        
        free(to_remove->filename);
        free(to_remove);

        wal->segment_count--;
    }

    wal->segments = curr;
    return PPDB_OK;
}

// 基础WAL操作实现
ppdb_error_t ppdb_wal_create_basic(const ppdb_wal_config_t* config, ppdb_wal_t** wal) {
    if (!config || !wal) return PPDB_ERR_INVALID_ARG;

    ppdb_wal_t* new_wal = calloc(1, sizeof(ppdb_wal_t));
    if (!new_wal) return PPDB_ERR_OUT_OF_MEMORY;

    // 复制配置
    memcpy(&new_wal->config, config, sizeof(ppdb_wal_config_t));
    new_wal->dir_path = strdup(config->dir_path);
    if (!new_wal->dir_path) {
        free(new_wal);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 创建目录
    if (mkdir(new_wal->dir_path, 0755) != 0 && errno != EEXIST) {
        free(new_wal->dir_path);
        free(new_wal);
        return PPDB_ERR_IO;
    }

    // 初始化同步对象
    new_wal->sync = malloc(sizeof(ppdb_sync_t));
    if (!new_wal->sync) {
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
        free(new_wal->dir_path);
        free(new_wal);
        return err;
    }

    // 扫描现有段
    err = scan_existing_segments(new_wal);
    if (err != PPDB_OK) {
        ppdb_sync_destroy(new_wal->sync);
        free(new_wal->sync);
        free(new_wal->dir_path);
        free(new_wal);
        return err;
    }

    // 如果没有段，创建第一个段
    if (!new_wal->segments) {
        wal_segment_t* first_segment;
        err = create_new_segment(new_wal, &first_segment);
        if (err != PPDB_OK) {
            ppdb_sync_destroy(new_wal->sync);
            free(new_wal->sync);
            free(new_wal->dir_path);
            free(new_wal);
            return err;
        }
    }

    new_wal->closed = false;
    *wal = new_wal;
    return PPDB_OK;
}

void ppdb_wal_destroy_basic(ppdb_wal_t* wal) {
    if (!wal) return;

    // 清理所有段
    wal_segment_t* curr = wal->segments;
    while (curr) {
        wal_segment_t* next = curr->next;
        if (curr->fd >= 0) {
            close(curr->fd);
        }
        free(curr->filename);
        free(curr);
        curr = next;
    }

    if (wal->sync) {
        ppdb_sync_destroy(wal->sync);
        free(wal->sync);
    }

    free(wal->write_buffer);
    free(wal->dir_path);
    free(wal);
}

// 写入记录到当前段
ppdb_error_t write_record_to_segment(wal_segment_t* segment, const void* key, size_t key_size,
                                          const void* value, size_t value_size, uint64_t sequence) {
    if (!segment || !key || !value) return PPDB_ERR_INVALID_ARG;

    // 计算记录大小
    size_t record_size = sizeof(wal_record_header_t) + key_size + value_size;
    
    // 准备记录头部
    wal_record_header_t header = {
        .magic = WAL_MAGIC,
        .key_size = key_size,
        .value_size = value_size,
        .sequence = sequence,
        .checksum = 0
    };

    // 计算记录校验和
    header.checksum = calculate_crc32(&header, sizeof(header));
    header.checksum = calculate_crc32(key, key_size);
    header.checksum = calculate_crc32(value, value_size);

    // 写入记录头部
    ssize_t written = pwrite(segment->fd, &header, sizeof(header), segment->size);
    if (written != sizeof(header)) {
        return PPDB_ERR_IO;
    }

    // 写入键
    written = pwrite(segment->fd, key, key_size, segment->size + sizeof(header));
    if (written != key_size) {
        return PPDB_ERR_IO;
    }

    // 写入值
    written = pwrite(segment->fd, value, value_size, 
                    segment->size + sizeof(header) + key_size);
    if (written != value_size) {
        return PPDB_ERR_IO;
    }

    // 更新段信息
    segment->size += record_size;
    segment->last_sequence = sequence;

    return PPDB_OK;
}

// 写入记录
ppdb_error_t ppdb_wal_write_basic(ppdb_wal_t* wal, const void* key, size_t key_size,
                                 const void* value, size_t value_size) {
    if (!wal || !key || !value || wal->closed) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err = PPDB_OK;
    bool need_new_segment = false;

    // 获取锁
    err = ppdb_sync_lock(wal->sync);
    if (err != PPDB_OK) {
        return err;
    }

    // 获取当前活动段
    wal_segment_t* curr_segment = wal->segments;
    while (curr_segment && curr_segment->next) {
        curr_segment = curr_segment->next;
    }

    // 检查是否需要新段
    if (!curr_segment || curr_segment->is_sealed || 
        curr_segment->size + sizeof(wal_record_header_t) + key_size + value_size > 
        wal->config.segment_size) {
        need_new_segment = true;
    }

    // 如果需要，创建新段
    if (need_new_segment) {
        if (curr_segment) {
            err = seal_segment(curr_segment);
            if (err != PPDB_OK) {
                ppdb_sync_unlock(wal->sync);
                return err;
            }
        }

        err = create_new_segment(wal, &curr_segment);
        if (err != PPDB_OK) {
            ppdb_sync_unlock(wal->sync);
            return err;
        }

        // 清理旧段
        err = cleanup_old_segments(wal);
        if (err != PPDB_OK) {
            ppdb_sync_unlock(wal->sync);
            return err;
        }
    }

    // 写入记录
    err = write_record_to_segment(curr_segment, key, key_size, value, value_size, 
                                wal->next_sequence++);
    if (err != PPDB_OK) {
        ppdb_sync_unlock(wal->sync);
        return err;
    }

    // 如果需要同步
    if (wal->config.sync_write) {
        if (fsync(curr_segment->fd) != 0) {
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

    wal_segment_t* curr = wal->segments;

    // 遍历所有段
    while (curr) {
        if (!curr->is_sealed) {
            if (fsync(curr->fd) != 0) {
                return PPDB_ERR_IO;
            }
        }
        curr = curr->next;
    }

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

ppdb_error_t ppdb_wal_write(ppdb_wal_t* wal, const void* key, size_t key_size,
                           const void* value, size_t value_size) {
    return ppdb_wal_write_basic(wal, key, key_size, value, value_size);
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

// 获取段数量
size_t ppdb_wal_segment_count(ppdb_wal_t* wal) {
    if (!wal) return 0;
    return wal->segment_count;
}

// 读取记录
static ppdb_error_t read_record(int fd, size_t offset, void** key, size_t* key_size,
                               void** value, size_t* value_size, uint64_t* sequence) {
    wal_record_header_t header;
    ssize_t read_size;

    // 读取记录头部
    read_size = pread(fd, &header, sizeof(header), offset);
    if (read_size != sizeof(header)) {
        return PPDB_ERR_IO;
    }

    // 验证魔数
    if (header.magic != WAL_MAGIC) {
        return PPDB_ERR_WAL_CORRUPTED;
    }

    // 分配内存并读取键
    *key = malloc(header.key_size);
    if (!*key) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    read_size = pread(fd, *key, header.key_size, offset + sizeof(header));
    if (read_size != header.key_size) {
        free(*key);
        return PPDB_ERR_IO;
    }

    // 分配内存并读取值
    *value = malloc(header.value_size);
    if (!*value) {
        free(*key);
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    read_size = pread(fd, *value, header.value_size, 
                     offset + sizeof(header) + header.key_size);
    if (read_size != header.value_size) {
        free(*key);
        free(*value);
        return PPDB_ERR_IO;
    }

    // 验证校验和
    uint32_t saved_checksum = header.checksum;
    header.checksum = 0;
    uint32_t computed_checksum = calculate_crc32(&header, sizeof(header));
    computed_checksum = calculate_crc32(*key, header.key_size);
    computed_checksum = calculate_crc32(*value, header.value_size);
    if (computed_checksum != saved_checksum) {
        free(*key);
        free(*value);
        return PPDB_ERR_WAL_CORRUPTED;
    }

    *key_size = header.key_size;
    *value_size = header.value_size;
    *sequence = header.sequence;

    return PPDB_OK;
}

// 创建迭代器
ppdb_error_t ppdb_wal_iterator_create(ppdb_wal_t* wal, ppdb_wal_iterator_t** iterator) {
    if (!wal || !iterator) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_wal_iterator_t* new_iterator = calloc(1, sizeof(ppdb_wal_iterator_t));
    if (!new_iterator) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    new_iterator->wal = wal;
    new_iterator->curr_segment = wal->segments;  // 从第一个段开始
    new_iterator->curr_offset = WAL_SEGMENT_HEADER_SIZE;  // 跳过段头部
    new_iterator->valid = true;
    new_iterator->read_buffer = malloc(WAL_BUFFER_SIZE);
    if (!new_iterator->read_buffer) {
        free(new_iterator);
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    new_iterator->buffer_size = WAL_BUFFER_SIZE;

    *iterator = new_iterator;
    return PPDB_OK;
}

// 销毁迭代器
void ppdb_wal_iterator_destroy(ppdb_wal_iterator_t* iterator) {
    if (!iterator) {
        return;
    }
    free(iterator->read_buffer);
    free(iterator);
}

// 迭代器是否有效
bool ppdb_wal_iterator_valid(ppdb_wal_iterator_t* iterator) {
    return iterator && iterator->valid;
}

// 迭代器移动到下一条记录
ppdb_error_t ppdb_wal_iterator_next(ppdb_wal_iterator_t* iterator) {
    if (!iterator || !iterator->valid) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 如果当前段无效，移动到下一个段
    while (iterator->curr_segment) {
        // 读取当前记录头部
        wal_record_header_t header;
        ssize_t read_size = pread(iterator->curr_segment->fd, &header, 
                                sizeof(header), iterator->curr_offset);
        
        if (read_size == sizeof(header) && header.magic == WAL_MAGIC) {
            // 移动到下一条记录
            iterator->curr_offset += sizeof(header) + header.key_size + header.value_size;
            return PPDB_OK;
        }

        // 当前段已读完，移动到下一个段
        iterator->curr_segment = iterator->curr_segment->next;
        if (iterator->curr_segment) {
            iterator->curr_offset = WAL_SEGMENT_HEADER_SIZE;
        } else {
            iterator->valid = false;
            break;
        }
    }

    return PPDB_OK;
}

// 获取当前记录
ppdb_error_t ppdb_wal_iterator_get(ppdb_wal_iterator_t* iterator, void** key, 
                                  size_t* key_size, void** value, size_t* value_size) {
    if (!iterator || !iterator->valid || !key || !key_size || !value || !value_size) {
        return PPDB_ERR_INVALID_ARG;
    }

    uint64_t sequence;
    return read_record(iterator->curr_segment->fd, iterator->curr_offset,
                      key, key_size, value, value_size, &sequence);
}

// 恢复 WAL 数据
ppdb_error_t ppdb_wal_recover(ppdb_wal_t* wal, ppdb_memtable_t* memtable) {
    if (!wal || !memtable) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err = PPDB_OK;
    ppdb_wal_iterator_t* iterator = NULL;

    // 创建迭代器
    err = ppdb_wal_iterator_create(wal, &iterator);
    if (err != PPDB_OK) {
        return err;
    }

    // 遍历所有记录
    while (ppdb_wal_iterator_valid(iterator)) {
        void* key = NULL;
        void* value = NULL;
        size_t key_size = 0;
        size_t value_size = 0;

        // 获取当前记录
        err = ppdb_wal_iterator_get(iterator, &key, &key_size, &value, &value_size);
        if (err != PPDB_OK) {
            ppdb_wal_iterator_destroy(iterator);
            return err;
        }

        // 写入内存表
        err = ppdb_memtable_put(memtable, key, key_size, value, value_size);
        free(key);
        free(value);
        if (err != PPDB_OK) {
            ppdb_wal_iterator_destroy(iterator);
            return err;
        }

        // 移动到下一条记录
        err = ppdb_wal_iterator_next(iterator);
        if (err != PPDB_OK) {
            ppdb_wal_iterator_destroy(iterator);
            return err;
        }
    }

    ppdb_wal_iterator_destroy(iterator);
    return PPDB_OK;
}

// 清理过期的 WAL 段
ppdb_error_t ppdb_wal_cleanup(ppdb_wal_t* wal, uint64_t min_sequence) {
    if (!wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    wal_segment_t* curr = wal->segments;
    wal_segment_t* prev = NULL;

    // 遍历所有段
    while (curr) {
        // 如果段的最后一个序列号小于最小序列号，可以删除该段
        if (curr->last_sequence < min_sequence) {
            wal_segment_t* to_delete = curr;
            curr = curr->next;

            // 更新链表
            if (prev) {
                prev->next = curr;
            } else {
                wal->segments = curr;
            }

            // 关闭并删除文件
            close(to_delete->fd);
            unlink(to_delete->filename);
            free(to_delete->filename);
            free(to_delete);
            wal->segment_count--;
        } else {
            prev = curr;
            curr = curr->next;
        }
    }

    return PPDB_OK;
}

// 获取 WAL 统计信息
ppdb_error_t ppdb_wal_stats(ppdb_wal_t* wal, ppdb_wal_stats_t* stats) {
    if (!wal || !stats) {
        return PPDB_ERR_INVALID_ARG;
    }

    memset(stats, 0, sizeof(ppdb_wal_stats_t));

    // 遍历所有段统计信息
    wal_segment_t* curr = wal->segments;
    while (curr) {
        stats->total_segments++;
        stats->total_size += curr->size;
        if (curr->is_sealed) {
            stats->sealed_segments++;
        }
        curr = curr->next;
    }

    return PPDB_OK;
}

// 获取最小和最大序列号
ppdb_error_t ppdb_wal_get_sequence_range(ppdb_wal_t* wal, uint64_t* min_sequence, 
                                        uint64_t* max_sequence) {
    if (!wal || !min_sequence || !max_sequence) {
        return PPDB_ERR_INVALID_ARG;
    }

    *min_sequence = UINT64_MAX;
    *max_sequence = 0;

    // 遍历所有段找到最小和最大序列号
    wal_segment_t* curr = wal->segments;
    while (curr) {
        if (curr->first_sequence < *min_sequence) {
            *min_sequence = curr->first_sequence;
        }
        if (curr->last_sequence > *max_sequence) {
            *max_sequence = curr->last_sequence;
        }
        curr = curr->next;
    }

    // 如果没有找到任何记录
    if (*min_sequence == UINT64_MAX) {
        *min_sequence = 0;
        *max_sequence = 0;
    }

    return PPDB_OK;
}

// 检查 WAL 是否需要切换到新段
bool ppdb_wal_need_roll(ppdb_wal_t* wal) {
    if (!wal || !wal->segments) {
        return true;
    }

    // 获取当前活动段
    wal_segment_t* curr = wal->segments;
    while (curr->next) {
        curr = curr->next;
    }

    // 如果当前段已封存或接近大小限制，需要切换
    return curr->is_sealed || 
           curr->size + WAL_BUFFER_SIZE > wal->config.segment_size;
}

// 获取 WAL 目录路径
const char* ppdb_wal_get_path(ppdb_wal_t* wal) {
    return wal ? wal->dir_path : NULL;
}

// 获取 WAL 段数量
size_t ppdb_wal_get_segment_count(ppdb_wal_t* wal) {
    return wal ? wal->segment_count : 0;
}

// 获取 WAL 总大小
size_t ppdb_wal_get_total_size(ppdb_wal_t* wal) {
    if (!wal) {
        return 0;
    }

    size_t total_size = 0;
    wal_segment_t* curr = wal->segments;
    while (curr) {
        total_size += curr->size;
        curr = curr->next;
    }

    return total_size;
}

// 检查 WAL 是否已关闭
bool ppdb_wal_is_closed(ppdb_wal_t* wal) {
    return !wal || wal->closed;
}

// 获取 WAL 配置信息
const ppdb_wal_config_t* ppdb_wal_get_config(ppdb_wal_t* wal) {
    return wal ? &wal->config : NULL;
}
