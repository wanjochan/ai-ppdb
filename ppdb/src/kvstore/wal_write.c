#include <cosmopolitan.h>
#include "ppdb/ppdb_kvstore.h"
#include "ppdb/ppdb_error.h"
#include "kvstore/internal/kvstore_wal.h"
#include "kvstore/internal/kvstore_wal_types.h"
#include "ppdb/ppdb_logger.h"

// 前向声明
static ppdb_error_t seal_segment(wal_segment_t* segment);
static ppdb_error_t create_new_segment(ppdb_wal_t* wal, wal_segment_t** segment);
static ppdb_error_t cleanup_old_segments(ppdb_wal_t* wal);

// 封存段
static ppdb_error_t seal_segment(wal_segment_t* segment) {
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

// 创建新段
static ppdb_error_t create_new_segment(ppdb_wal_t* wal, wal_segment_t** segment) {
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

// 清理旧段
static ppdb_error_t cleanup_old_segments(ppdb_wal_t* wal) {
    if (!wal || wal->segment_count <= wal->config.max_segments) {
        return PPDB_OK;
    }

    // 保留最新的段
    size_t segments_to_remove = wal->segment_count - wal->config.max_segments;
    wal_segment_t* current = wal->segments;
    wal_segment_t* prev = NULL;

    for (size_t i = 0; i < segments_to_remove && current; i++) {
        wal_segment_t* to_remove = current;
        current = current->next;

        // 关闭并删除文件
        HANDLE file_handle = (HANDLE)_get_osfhandle(to_remove->fd);
        if (file_handle != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(file_handle);
            CloseHandle(file_handle);
        }
        close(to_remove->fd);
        DeleteFile(to_remove->filename);
        free(to_remove->filename);
        free(to_remove);

        wal->segment_count--;
        wal->total_size -= to_remove->size;
    }

    // 更新段链表头
    wal->segments = current;
    return PPDB_OK;
}

// 切换到新段
ppdb_error_t roll_new_segment(ppdb_wal_t* wal) {
    if (!wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 封存当前段
    if (wal->segments) {
        wal_segment_t* curr = wal->segments;
        while (curr->next) {
            curr = curr->next;
        }

        // 获取当前段的文件句柄
        HANDLE curr_handle = (HANDLE)_get_osfhandle(curr->fd);
        if (curr_handle != INVALID_HANDLE_VALUE) {
            // 确保所有数据都写入磁盘
            FlushFileBuffers(curr_handle);
        }

        ppdb_error_t err = seal_segment(curr);
        if (err != PPDB_OK) {
            return err;
        }
    }

    // 创建新段
    wal_segment_t* new_segment;
    ppdb_error_t err = create_new_segment(wal, &new_segment);
    if (err != PPDB_OK) {
        return err;
    }

    // 获取新段的文件句柄
    HANDLE new_handle = (HANDLE)_get_osfhandle(new_segment->fd);
    if (new_handle == INVALID_HANDLE_VALUE) {
        return PPDB_ERR_IO;
    }

    // 更新当前段信息
    wal->current_fd = new_segment->fd;
    wal->current_size = new_segment->size;

    // 清理旧段
    err = cleanup_old_segments(wal);
    if (err != PPDB_OK) {
        return err;
    }

    return PPDB_OK;
}

// 写入单条记录
static ppdb_error_t write_record(ppdb_wal_t* wal, const void* key, size_t key_size,
                               const void* value, size_t value_size) {
    if (!wal || !key || !value) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 准备记录头部
    wal_record_header_t header = {
        .magic = WAL_MAGIC,
        .type = PPDB_WAL_RECORD_PUT,
        .key_size = key_size,
        .value_size = value_size,
        .sequence = wal->next_sequence++,
        .checksum = 0
    };

    // 计算校验和
    uint32_t checksum = 0;
    checksum = calculate_crc32(&header, sizeof(header));
    checksum = calculate_crc32_update(checksum, key, key_size);
    checksum = calculate_crc32_update(checksum, value, value_size);
    header.checksum = checksum;

    // 获取文件句柄
    HANDLE file_handle = (HANDLE)_get_osfhandle(wal->current_fd);
    if (file_handle == INVALID_HANDLE_VALUE) {
        return PPDB_ERR_IO;
    }

    // 复制文件句柄
    HANDLE dup_handle;
    if (!DuplicateHandle(GetCurrentProcess(), file_handle,
                        GetCurrentProcess(), &dup_handle,
                        GENERIC_READ | GENERIC_WRITE,
                        FALSE,
                        0)) {
        return PPDB_ERR_IO;
    }

    // 创建事件对象
    SECURITY_ATTRIBUTES sa = {
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .lpSecurityDescriptor = NULL,
        .bInheritHandle = FALSE
    };
    HANDLE event_handle = CreateEvent(&sa, TRUE, FALSE, NULL);
    if (event_handle == NULL) {
        CloseHandle(dup_handle);
        return PPDB_ERR_IO;
    }

    // 锁定文件
    OVERLAPPED overlapped = {0};
    overlapped.hEvent = event_handle;

    // 尝试获取锁
    DWORD start_time = GetTickCount();
    while (TRUE) {
        if (LockFileEx(dup_handle, LOCKFILE_EXCLUSIVE_LOCK, 0, MAXDWORD, MAXDWORD, &overlapped)) {
            break;
        }

        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING) {
            // 等待锁定完成
            DWORD wait_result = WaitForSingleObject(event_handle, 100);  // 等待100毫秒
            if (wait_result == WAIT_OBJECT_0) {
                break;
            } else if (wait_result == WAIT_TIMEOUT) {
                // 检查是否超时
                if (GetTickCount() - start_time > 5000) {  // 5秒超时
                    CloseHandle(event_handle);
                    CloseHandle(dup_handle);
                    return PPDB_ERR_TIMEOUT;
                }
                continue;
            } else {
                CloseHandle(event_handle);
                CloseHandle(dup_handle);
                return PPDB_ERR_IO;
            }
        } else {
            CloseHandle(event_handle);
            CloseHandle(dup_handle);
            return PPDB_ERR_IO;
        }
    }

    ppdb_error_t err = PPDB_OK;

    // 读取段头部
    wal_segment_header_t seg_header;
    DWORD bytes_read;
    LARGE_INTEGER offset = {0};
    if (!SetFilePointerEx(dup_handle, offset, NULL, FILE_BEGIN) ||
        !ReadFile(dup_handle, &seg_header, sizeof(seg_header), &bytes_read, NULL) ||
        bytes_read != sizeof(seg_header)) {
        UnlockFileEx(dup_handle, 0, MAXDWORD, MAXDWORD, &overlapped);
        CloseHandle(event_handle);
        CloseHandle(dup_handle);
        return PPDB_ERR_IO;
    }

    // 验证段头部
    if (seg_header.magic != WAL_MAGIC || seg_header.version != WAL_VERSION) {
        UnlockFileEx(dup_handle, 0, MAXDWORD, MAXDWORD, &overlapped);
        CloseHandle(event_handle);
        CloseHandle(dup_handle);
        return PPDB_ERR_WAL_CORRUPTED;
    }

    // 移动到文件末尾
    offset.QuadPart = wal->current_size;
    if (!SetFilePointerEx(dup_handle, offset, NULL, FILE_BEGIN)) {
        UnlockFileEx(dup_handle, 0, MAXDWORD, MAXDWORD, &overlapped);
        CloseHandle(event_handle);
        CloseHandle(dup_handle);
        return PPDB_ERR_IO;
    }

    // 准备写入缓冲区
    size_t total_size = sizeof(header) + key_size + value_size;
    size_t aligned_size = (total_size + 511) & ~511;  // 向上对齐到512字节
    void* aligned_buffer = _aligned_malloc(aligned_size, 512);
    if (!aligned_buffer) {
        UnlockFileEx(dup_handle, 0, MAXDWORD, MAXDWORD, &overlapped);
        CloseHandle(event_handle);
        CloseHandle(dup_handle);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 复制数据到缓冲区
    memcpy(aligned_buffer, &header, sizeof(header));
    memcpy((char*)aligned_buffer + sizeof(header), key, key_size);
    memcpy((char*)aligned_buffer + sizeof(header) + key_size, value, value_size);
    if (aligned_size > total_size) {
        memset((char*)aligned_buffer + total_size, 0, aligned_size - total_size);
    }

    // 写入数据
    DWORD bytes_written;
    if (!WriteFile(dup_handle, aligned_buffer, aligned_size, &bytes_written, NULL) ||
        bytes_written != aligned_size) {
        _aligned_free(aligned_buffer);
        UnlockFileEx(dup_handle, 0, MAXDWORD, MAXDWORD, &overlapped);
        CloseHandle(event_handle);
        CloseHandle(dup_handle);
        return PPDB_ERR_IO;
    }

    _aligned_free(aligned_buffer);

    // 更新当前段大小和序号
    wal->current_size += total_size;
    wal->total_size += total_size;

    // 更新当前段的最后序号
    wal_segment_t* curr = wal->segments;
    while (curr && curr->next) {
        curr = curr->next;
    }
    if (curr) {
        curr->last_sequence = header.sequence;
        curr->size = wal->current_size;
        
        // 更新段头部
        seg_header.last_sequence = header.sequence;
        seg_header.record_count++;
        seg_header.checksum = 0;
        seg_header.checksum = calculate_crc32(&seg_header, sizeof(seg_header));
        
        // 分配对齐的缓冲区用于头部
        void* aligned_header_buffer = _aligned_malloc(512, 512);
        if (!aligned_header_buffer) {
            UnlockFileEx(dup_handle, 0, MAXDWORD, MAXDWORD, &overlapped);
            CloseHandle(event_handle);
            CloseHandle(dup_handle);
            return PPDB_ERR_OUT_OF_MEMORY;
        }

        // 复制头部到对齐的缓冲区
        memcpy(aligned_header_buffer, &seg_header, sizeof(seg_header));
        memset((char*)aligned_header_buffer + sizeof(seg_header), 0, 512 - sizeof(seg_header));

        // 移动到文件开头
        offset.QuadPart = 0;
        if (!SetFilePointerEx(dup_handle, offset, NULL, FILE_BEGIN) ||
            !WriteFile(dup_handle, aligned_header_buffer, 512, &bytes_written, NULL) ||
            bytes_written != 512) {
            _aligned_free(aligned_header_buffer);
            UnlockFileEx(dup_handle, 0, MAXDWORD, MAXDWORD, &overlapped);
            CloseHandle(event_handle);
            CloseHandle(dup_handle);
            return PPDB_ERR_IO;
        }

        _aligned_free(aligned_header_buffer);

        // 确保段头部写入磁盘
        FlushFileBuffers(dup_handle);

        // 移动回文件末尾
        offset.QuadPart = wal->current_size;
        if (!SetFilePointerEx(dup_handle, offset, NULL, FILE_BEGIN)) {
            UnlockFileEx(dup_handle, 0, MAXDWORD, MAXDWORD, &overlapped);
            CloseHandle(event_handle);
            CloseHandle(dup_handle);
            return PPDB_ERR_IO;
        }
    }

    // 如果需要同步写入
    if (wal->sync_on_write) {
        if (!FlushFileBuffers(dup_handle)) {
            UnlockFileEx(dup_handle, 0, MAXDWORD, MAXDWORD, &overlapped);
            CloseHandle(event_handle);
            CloseHandle(dup_handle);
            return PPDB_ERR_IO;
        }
    }

    // 解锁文件
    UnlockFileEx(dup_handle, 0, MAXDWORD, MAXDWORD, &overlapped);
    CloseHandle(event_handle);
    CloseHandle(dup_handle);

    return PPDB_OK;
}

// 批量写入
ppdb_error_t ppdb_wal_write_batch(ppdb_wal_t* wal, const ppdb_write_batch_t* batch) {
    if (!wal || !batch || !batch->ops || batch->count == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (wal->closed) {
        return PPDB_ERR_WAL_CLOSED;
    }

    ppdb_error_t err = PPDB_OK;
    ppdb_sync_lock(wal->sync);

    // 计算批量写入的总大小
    size_t total_size = 0;
    for (size_t i = 0; i < batch->count; i++) {
        total_size += sizeof(wal_record_header_t) + 
                     batch->ops[i].key_size + 
                     batch->ops[i].value_size;
    }

    // 检查是否需要切换到新段
    if (wal->current_size + total_size > wal->config.segment_size) {
        err = roll_new_segment(wal);
        if (err != PPDB_OK) {
            ppdb_sync_unlock(wal->sync);
            return err;
        }
    }

    // 写入所有记录
    for (size_t i = 0; i < batch->count; i++) {
        err = write_record(wal, batch->ops[i].key, batch->ops[i].key_size,
                          batch->ops[i].value, batch->ops[i].value_size);
        if (err != PPDB_OK) {
            ppdb_sync_unlock(wal->sync);
            return err;
        }
    }

    ppdb_sync_unlock(wal->sync);
    return PPDB_OK;
}

// 无锁批量写入
ppdb_error_t ppdb_wal_write_batch_lockfree(ppdb_wal_t* wal, const ppdb_write_batch_t* batch) {
    if (!wal || !batch || !batch->ops || batch->count == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (wal->closed) {
        return PPDB_ERR_WAL_CLOSED;
    }

    ppdb_error_t err = PPDB_OK;

    // 计算批量写入的总大小
    size_t total_size = 0;
    for (size_t i = 0; i < batch->count; i++) {
        total_size += sizeof(wal_record_header_t) + 
                     batch->ops[i].key_size + 
                     batch->ops[i].value_size;
    }

    // 检查是否需要切换到新段
    if (wal->current_size + total_size > wal->config.segment_size) {
        err = roll_new_segment(wal);
        if (err != PPDB_OK) {
            return err;
        }
    }

    // 写入所有记录
    for (size_t i = 0; i < batch->count; i++) {
        err = write_record(wal, batch->ops[i].key, batch->ops[i].key_size,
                          batch->ops[i].value, batch->ops[i].value_size);
        if (err != PPDB_OK) {
            return err;
        }
    }

    return PPDB_OK;
}

// 写入单条记录（基本实现）
ppdb_error_t ppdb_wal_write_basic(ppdb_wal_t* wal, const void* key, size_t key_size,
                                 const void* value, size_t value_size) {
    if (!wal || !key || !value) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (wal->closed) {
        return PPDB_ERR_WAL_CLOSED;
    }

    // 检查是否需要切换到新段
    size_t record_size = sizeof(wal_record_header_t) + key_size + value_size;
    if (wal->current_size + record_size > wal->config.segment_size) {
        ppdb_error_t err = roll_new_segment(wal);
        if (err != PPDB_OK) {
            return err;
        }
    }

    // 写入记录
    return write_record(wal, key, key_size, value, value_size);
}

// 写入单条记录（带锁）
ppdb_error_t ppdb_wal_write(ppdb_wal_t* wal, const void* key, size_t key_size,
                           const void* value, size_t value_size) {
    if (!wal || !key || !value || key_size == 0 || value_size == 0) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (wal->closed) {
        return PPDB_ERR_WAL_CLOSED;
    }

    ppdb_error_t err = PPDB_OK;
    ppdb_sync_lock(wal->sync);

    // 检查是否需要切换到新段
    size_t record_size = sizeof(wal_record_header_t) + key_size + value_size;
    size_t aligned_size = (record_size + 511) & ~511;  // 向上对齐到512字节
    if (wal->current_size + aligned_size > wal->config.segment_size) {
        err = roll_new_segment(wal);
        if (err != PPDB_OK) {
            ppdb_sync_unlock(wal->sync);
            return err;
        }
    }

    // 写入记录
    err = write_record(wal, key, key_size, value, value_size);
    ppdb_sync_unlock(wal->sync);
    return err;
}

// 写入单条记录（无锁）
ppdb_error_t ppdb_wal_write_lockfree(ppdb_wal_t* wal, const void* key, size_t key_size,
                                    const void* value, size_t value_size) {
    if (!wal || !key || !value) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (wal->closed) {
        return PPDB_ERR_WAL_CLOSED;
    }

    // 检查是否需要切换到新段
    size_t record_size = sizeof(wal_record_header_t) + key_size + value_size;
    if (wal->current_size + record_size > wal->config.segment_size) {
        ppdb_error_t err = roll_new_segment(wal);
        if (err != PPDB_OK) {
            return err;
        }
    }

    // 写入记录
    return write_record(wal, key, key_size, value, value_size);
} 