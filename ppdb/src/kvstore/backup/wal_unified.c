#include "wal_unified.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>

// CRC32计算函数
static uint32_t calculate_crc32(const void* data, size_t len) {
    // TODO: 实现CRC32计算
    return 0;
}

// 获取当前时间戳(ms)
static uint64_t get_current_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// 组提交线程函数
static void* group_commit_thread(void* arg) {
    ppdb_wal_t* wal = (ppdb_wal_t*)arg;
    
    while (!atomic_load(&wal->is_closing)) {
        usleep(wal->group_commit.interval * 1000);
        
        // 检查是否需要提交
        if (wal->write_buffer.used > 0) {
            ppdb_wal_flush(wal);
        }
    }
    
    return NULL;
}

// 创建WAL
ppdb_wal_t* ppdb_wal_create(const char* filename,
                           const ppdb_wal_config_t* config) {
    ppdb_wal_t* wal = calloc(1, sizeof(ppdb_wal_t));
    if (!wal) return NULL;
    
    // 复制配置
    memcpy(&wal->config, config, sizeof(ppdb_wal_config_t));
    
    // 打开文件
    wal->fd = open(filename, O_CREAT | O_RDWR | O_APPEND, 0644);
    if (wal->fd < 0) {
        free(wal);
        return NULL;
    }
    
    wal->filename = strdup(filename);
    
    // 初始化同步机制
    ppdb_sync_init(&wal->sync, &config->sync_config);
    ppdb_sync_init(&wal->write_buffer.buffer_lock, &config->sync_config);
    
    // 初始化写缓冲
    wal->write_buffer.buffer = malloc(config->buffer_size);
    wal->write_buffer.size = config->buffer_size;
    wal->write_buffer.used = 0;
    
    // 初始化组提交
    if (config->enable_group_commit) {
        wal->group_commit.enabled = true;
        wal->group_commit.interval = config->group_commit_interval;
        wal->group_commit.last_commit = get_current_time_ms();
        
        // 创建组提交线程
        pthread_t* thread = malloc(sizeof(pthread_t));
        pthread_create(thread, NULL, group_commit_thread, wal);
        wal->group_commit.thread = thread;
    }
    
    atomic_init(&wal->is_closing, false);
    
    return wal;
}

void ppdb_wal_destroy(ppdb_wal_t* wal) {
    if (!wal) return;
    
    // 停止组提交线程
    if (wal->group_commit.enabled) {
        atomic_store(&wal->is_closing, true);
        pthread_join(*(pthread_t*)wal->group_commit.thread, NULL);
        free(wal->group_commit.thread);
    }
    
    // 刷新剩余数据
    ppdb_wal_flush(wal);
    
    // 清理资源
    ppdb_sync_destroy(&wal->sync);
    ppdb_sync_destroy(&wal->write_buffer.buffer_lock);
    
    if (wal->write_buffer.buffer) {
        free(wal->write_buffer.buffer);
    }
    
    if (wal->filename) {
        free(wal->filename);
    }
    
    if (wal->fd >= 0) {
        close(wal->fd);
    }
    
    free(wal);
}

// 写入记录
int ppdb_wal_append(ppdb_wal_t* wal,
                    ppdb_wal_record_type_t type,
                    const void* key, size_t key_size,
                    const void* value, size_t value_size,
                    uint64_t sequence) {
    // 计算记录大小
    size_t record_size = sizeof(ppdb_wal_record_header_t) + key_size + value_size;
    
    // 准备记录头
    ppdb_wal_record_header_t header = {
        .type = type,
        .key_size = key_size,
        .value_size = value_size,
        .sequence = sequence,
        .checksum = 0
    };
    
    if (wal->config.enable_checksum) {
        header.checksum = calculate_crc32(key, key_size);
        if (value && value_size > 0) {
            header.checksum ^= calculate_crc32(value, value_size);
        }
    }
    
    // 获取缓冲区锁
    ppdb_sync_lock(&wal->write_buffer.buffer_lock);
    
    // 检查缓冲区空间
    if (wal->write_buffer.used + record_size > wal->write_buffer.size) {
        // 缓冲区满，需要刷盘
        ppdb_wal_flush(wal);
    }
    
    // 写入缓冲区
    char* dst = (char*)wal->write_buffer.buffer + wal->write_buffer.used;
    memcpy(dst, &header, sizeof(header));
    dst += sizeof(header);
    memcpy(dst, key, key_size);
    dst += key_size;
    if (value && value_size > 0) {
        memcpy(dst, value, value_size);
    }
    
    wal->write_buffer.used += record_size;
    
    // 更新统计信息
    atomic_fetch_add(&wal->stats.total_writes, 1);
    atomic_fetch_add(&wal->stats.bytes_written, record_size);
    
    // 检查是否需要立即刷盘
    bool need_flush = false;
    if (!wal->config.enable_group_commit) {
        need_flush = true;
    } else {
        uint64_t now = get_current_time_ms();
        if (now - wal->group_commit.last_commit >= wal->group_commit.interval) {
            need_flush = true;
            wal->group_commit.last_commit = now;
        }
    }
    
    if (need_flush) {
        ppdb_wal_flush(wal);
    }
    
    ppdb_sync_unlock(&wal->write_buffer.buffer_lock);
    return PPDB_OK;
}

// 同步到磁盘
int ppdb_wal_sync(ppdb_wal_t* wal) {
    atomic_fetch_add(&wal->stats.sync_writes, 1);
    return fsync(wal->fd);
}

// 刷新缓冲区到文件
int ppdb_wal_flush(ppdb_wal_t* wal) {
    if (wal->write_buffer.used == 0) {
        return PPDB_OK;
    }
    
    // 写入文件
    ssize_t written = write(wal->fd, wal->write_buffer.buffer, 
                           wal->write_buffer.used);
    
    if (written != wal->write_buffer.used) {
        return PPDB_ERR_IO;
    }
    
    // 同步到磁盘
    if (!wal->config.enable_async_flush) {
        ppdb_wal_sync(wal);
    }
    
    // 重置缓冲区
    wal->write_buffer.used = 0;
    atomic_fetch_add(&wal->stats.flush_count, 1);
    
    return PPDB_OK;
}

// 恢复迭代器实现
ppdb_wal_recovery_iter_t* ppdb_wal_recovery_iter_create(ppdb_wal_t* wal) {
    ppdb_wal_recovery_iter_t* iter = malloc(sizeof(ppdb_wal_recovery_iter_t));
    if (!iter) return NULL;
    
    iter->wal = wal;
    iter->offset = 0;
    iter->buffer_size = 4096;  // 默认缓冲区大小
    iter->buffer = malloc(iter->buffer_size);
    
    if (!iter->buffer) {
        free(iter);
        return NULL;
    }
    
    return iter;
}

void ppdb_wal_recovery_iter_destroy(ppdb_wal_recovery_iter_t* iter) {
    if (!iter) return;
    if (iter->buffer) {
        free(iter->buffer);
    }
    free(iter);
}

bool ppdb_wal_recovery_iter_valid(ppdb_wal_recovery_iter_t* iter) {
    return iter && iter->offset < iter->wal->file_size;
}

int ppdb_wal_recovery_iter_next(ppdb_wal_recovery_iter_t* iter,
                               ppdb_wal_record_type_t* type,
                               void** key, size_t* key_size,
                               void** value, size_t* value_size,
                               uint64_t* sequence) {
    if (!ppdb_wal_recovery_iter_valid(iter)) {
        return PPDB_ERR_EOF;
    }
    
    // 读取记录头
    ppdb_wal_record_header_t header;
    ssize_t n = pread(iter->wal->fd, &header, sizeof(header), iter->offset);
    if (n != sizeof(header)) {
        return PPDB_ERR_CORRUPT;
    }
    
    // 检查记录大小
    size_t record_size = sizeof(header) + header.key_size + header.value_size;
    if (record_size > iter->buffer_size) {
        void* new_buffer = realloc(iter->buffer, record_size);
        if (!new_buffer) {
            return PPDB_ERR_NO_MEMORY;
        }
        iter->buffer = new_buffer;
        iter->buffer_size = record_size;
    }
    
    // 读取完整记录
    n = pread(iter->wal->fd, iter->buffer, record_size, iter->offset);
    if (n != record_size) {
        return PPDB_ERR_CORRUPT;
    }
    
    // 验证校验和
    if (iter->wal->config.enable_checksum) {
        uint32_t crc = calculate_crc32((char*)iter->buffer + sizeof(header),
                                     record_size - sizeof(header));
        if (crc != header.checksum) {
            return PPDB_ERR_CHECKSUM;
        }
    }
    
    // 设置返回值
    *type = header.type;
    *key = (char*)iter->buffer + sizeof(header);
    *key_size = header.key_size;
    *value = (char*)iter->buffer + sizeof(header) + header.key_size;
    *value_size = header.value_size;
    *sequence = header.sequence;
    
    // 更新偏移量
    iter->offset += record_size;
    
    return PPDB_OK;
}
