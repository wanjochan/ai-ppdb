#include <cosmopolitan.h>
#include "ppdb/ppdb_kvstore.h"
#include "ppdb/ppdb_error.h"
#include "kvstore/internal/kvstore_wal.h"
#include "kvstore/internal/kvstore_wal_types.h"
#include "common/logger.h"

// 清理过期的 WAL 段
ppdb_error_t ppdb_wal_cleanup(ppdb_wal_t* wal, uint64_t min_sequence) {
    if (!wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err = PPDB_OK;
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

// 强制同步 WAL 到磁盘
ppdb_error_t ppdb_wal_sync(ppdb_wal_t* wal) {
    if (!wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    ppdb_error_t err = PPDB_OK;
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

// 检查 WAL 段是否需要压缩
bool ppdb_wal_need_compact(ppdb_wal_t* wal) {
    if (!wal) {
        return false;
    }

    // 如果段数量超过阈值，需要压缩
    if (wal->segment_count > wal->config.max_segments) {
        return true;
    }

    // 如果总大小超过阈值，需要压缩
    size_t total_size = ppdb_wal_get_total_size(wal);
    if (total_size > wal->config.max_total_size) {
        return true;
    }

    return false;
}

// 压缩 WAL 段
ppdb_error_t ppdb_wal_compact(ppdb_wal_t* wal) {
    if (!wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 获取最小和最大序列号
    uint64_t min_sequence, max_sequence;
    ppdb_error_t err = ppdb_wal_get_sequence_range(wal, &min_sequence, &max_sequence);
    if (err != PPDB_OK) {
        return err;
    }

    // 计算需要保留的最小序列号
    uint64_t keep_sequence = max_sequence - wal->config.max_records + 1;
    if (keep_sequence < min_sequence) {
        keep_sequence = min_sequence;
    }

    // 清理旧段
    return ppdb_wal_cleanup(wal, keep_sequence);
}

// 获取 WAL 段信息
ppdb_error_t ppdb_wal_get_segment_info(ppdb_wal_t* wal, size_t index, 
                                      ppdb_wal_segment_info_t* info) {
    if (!wal || !info || index >= wal->segment_count) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 找到指定索引的段
    wal_segment_t* curr = wal->segments;
    for (size_t i = 0; i < index && curr; i++) {
        curr = curr->next;
    }

    if (!curr) {
        return PPDB_ERR_NOT_FOUND;
    }

    // 填充段信息
    info->id = curr->id;
    info->size = curr->size;
    info->is_sealed = curr->is_sealed;
    info->first_sequence = curr->first_sequence;
    info->last_sequence = curr->last_sequence;
    strncpy(info->filename, curr->filename, sizeof(info->filename) - 1);
    info->filename[sizeof(info->filename) - 1] = '\0';

    return PPDB_OK;
} 