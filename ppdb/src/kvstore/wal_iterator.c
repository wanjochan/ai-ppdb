#include <cosmopolitan.h>
#include "ppdb/ppdb_kvstore.h"
#include "ppdb/ppdb_error.h"
#include "kvstore/internal/kvstore_wal.h"
#include "kvstore/internal/kvstore_wal_types.h"
#include "common/logger.h"

// WAL 迭代器结构
struct ppdb_wal_iterator {
    ppdb_wal_t* wal;            // WAL实例
    wal_segment_t* curr_segment; // 当前段
    size_t curr_offset;          // 当前段内偏移
    bool valid;                  // 是否有效
    void* read_buffer;          // 读取缓冲区
    size_t buffer_size;         // 缓冲区大小
    uint64_t last_sequence;     // 最后读取的序列号
};

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

// 读取记录头部
static ppdb_error_t read_record_header(int fd, size_t offset, wal_record_header_t* header) {
    if (!header) {
        return PPDB_ERR_INVALID_ARG;
    }

    ssize_t read_size = pread(fd, header, sizeof(wal_record_header_t), offset);
    if (read_size != sizeof(wal_record_header_t)) {
        return PPDB_ERR_IO;
    }

    // 验证魔数
    if (header->magic != WAL_MAGIC) {
        return PPDB_ERR_CORRUPTED;
    }

    return PPDB_OK;
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
        ppdb_error_t err = read_record_header(iterator->curr_segment->fd, 
                                            iterator->curr_offset, &header);
        
        if (err == PPDB_OK) {
            // 记录有效，更新位置和序列号
            iterator->curr_offset += sizeof(header) + header.key_size + header.value_size;
            iterator->last_sequence = header.sequence;
            return PPDB_OK;
        }

        // 当前段已读完或损坏，移动到下一个段
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

    // 读取记录头部
    wal_record_header_t header;
    ppdb_error_t err = read_record_header(iterator->curr_segment->fd, 
                                        iterator->curr_offset, &header);
    if (err != PPDB_OK) {
        return err;
    }

    // 分配内存并读取键
    *key = malloc(header.key_size);
    if (!*key) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    ssize_t read_size = pread(iterator->curr_segment->fd, *key, header.key_size, 
                             iterator->curr_offset + sizeof(header));
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
    read_size = pread(iterator->curr_segment->fd, *value, header.value_size, 
                     iterator->curr_offset + sizeof(header) + header.key_size);
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
        return PPDB_ERR_CORRUPTED;
    }

    *key_size = header.key_size;
    *value_size = header.value_size;
    iterator->last_sequence = header.sequence;

    return PPDB_OK;
}

// 获取当前记录的序列号
uint64_t ppdb_wal_iterator_sequence(ppdb_wal_iterator_t* iterator) {
    return iterator ? iterator->last_sequence : 0;
}

// 重置迭代器到开始位置
ppdb_error_t ppdb_wal_iterator_reset(ppdb_wal_iterator_t* iterator) {
    if (!iterator || !iterator->wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    iterator->curr_segment = iterator->wal->segments;
    iterator->curr_offset = WAL_SEGMENT_HEADER_SIZE;
    iterator->valid = true;
    iterator->last_sequence = 0;

    return PPDB_OK;
}

// 跳转到指定序列号
ppdb_error_t ppdb_wal_iterator_seek(ppdb_wal_iterator_t* iterator, uint64_t sequence) {
    if (!iterator || !iterator->wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 从头开始查找
    iterator->curr_segment = iterator->wal->segments;
    iterator->curr_offset = WAL_SEGMENT_HEADER_SIZE;
    iterator->valid = true;

    // 遍历直到找到大于等于目标序列号的记录
    while (iterator->valid) {
        wal_record_header_t header;
        ppdb_error_t err = read_record_header(iterator->curr_segment->fd, 
                                            iterator->curr_offset, &header);
        if (err != PPDB_OK) {
            return err;
        }

        if (header.sequence >= sequence) {
            // 找到目标位置
            iterator->last_sequence = header.sequence;
            return PPDB_OK;
        }

        // 移动到下一条记录
        err = ppdb_wal_iterator_next(iterator);
        if (err != PPDB_OK) {
            return err;
        }
    }

    return PPDB_OK;
} 