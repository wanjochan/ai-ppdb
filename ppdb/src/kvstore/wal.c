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

// 创建 WAL
ppdb_error_t ppdb_wal_create(const ppdb_wal_config_t* config, ppdb_wal_t** wal) {
    return ppdb_wal_create_basic(config, wal);
}

// 销毁 WAL
void ppdb_wal_destroy(ppdb_wal_t* wal) {
    ppdb_wal_destroy_basic(wal);
}

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

// CRC32更新函数
uint32_t calculate_crc32_update(uint32_t crc, const void* data, size_t size) {
    if (!data || size == 0) return crc;
    return crc32c(crc, data, size);
}

// 生成段文件名
char* generate_segment_filename(const char* dir_path, uint64_t segment_id) {
    char* filename = malloc(strlen(dir_path) + 32);
    if (!filename) return NULL;
    snprintf(filename, strlen(dir_path) + 32, "%s\\wal-%06" PRIu64 ".log", dir_path, segment_id);
    return filename;
}

// 验证段的完整性
ppdb_error_t validate_segment(wal_segment_t* segment) {
    if (!segment || segment->fd < 0) return PPDB_ERR_INVALID_ARG;

    // 获取文件句柄
    HANDLE file_handle = (HANDLE)_get_osfhandle(segment->fd);
    if (file_handle == INVALID_HANDLE_VALUE) {
        return PPDB_ERR_IO;
    }

    // 读取段头部
    wal_segment_header_t header;
    DWORD bytes_read;
    LARGE_INTEGER offset;
    offset.QuadPart = 0;

    // 分配对齐的缓冲区
    void* aligned_buffer = _aligned_malloc(WAL_SEGMENT_HEADER_SIZE, 512);
    if (!aligned_buffer) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    if (!SetFilePointerEx(file_handle, offset, NULL, FILE_BEGIN) ||
        !ReadFile(file_handle, aligned_buffer, WAL_SEGMENT_HEADER_SIZE, &bytes_read, NULL) ||
        bytes_read != WAL_SEGMENT_HEADER_SIZE) {
        _aligned_free(aligned_buffer);
        return PPDB_ERR_IO;
    }

    // 复制数据到头部结构
    memcpy(&header, aligned_buffer, WAL_SEGMENT_HEADER_SIZE);
    _aligned_free(aligned_buffer);

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
    char search_path[PATH_MAX];
    snprintf(search_path, sizeof(search_path), "%s\\*", wal->dir_path);

    WIN32_FIND_DATA find_data;
    HANDLE find_handle = FindFirstFile(search_path, &find_data);
    if (find_handle == INVALID_HANDLE_VALUE) {
        return PPDB_OK;  // 目录为空
    }

    do {
        if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0) {
            continue;
        }

        // 检查是否是 WAL 段文件
        if (strstr(find_data.cFileName, ".log") == NULL) {
            continue;
        }

        // 解析段 ID
        uint64_t segment_id;
        if (sscanf(find_data.cFileName, "wal-%06" PRIu64 ".log", &segment_id) != 1) {
            continue;
        }

        // 创建新段
        wal_segment_t* segment = malloc(sizeof(wal_segment_t));
        if (!segment) {
            FindClose(find_handle);
            return PPDB_ERR_OUT_OF_MEMORY;
        }

        // 设置段信息
        segment->id = segment_id;
        segment->filename = malloc(strlen(wal->dir_path) + strlen(find_data.cFileName) + 2);
        if (!segment->filename) {
            free(segment);
            FindClose(find_handle);
            return PPDB_ERR_OUT_OF_MEMORY;
        }
        snprintf(segment->filename, strlen(wal->dir_path) + strlen(find_data.cFileName) + 2,
                "%s\\%s", wal->dir_path, find_data.cFileName);

        // 打开段文件
        HANDLE file_handle = CreateFile(segment->filename,
                                      GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ,
                                      NULL,
                                      OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL,
                                      NULL);
        if (file_handle == INVALID_HANDLE_VALUE) {
            free(segment->filename);
            free(segment);
            FindClose(find_handle);
            return PPDB_ERR_IO;
        }

        // 转换为文件描述符
        segment->fd = _open_osfhandle((intptr_t)file_handle, _O_RDWR | _O_BINARY);
        if (segment->fd < 0) {
            CloseHandle(file_handle);
            free(segment->filename);
            free(segment);
            FindClose(find_handle);
            return PPDB_ERR_IO;
        }

        // 验证段完整性
        ppdb_error_t err = validate_segment(segment);
        if (err != PPDB_OK) {
            close(segment->fd);
            free(segment->filename);
            free(segment);
            FindClose(find_handle);
            return err;
        }

        // 更新段大小
        LARGE_INTEGER size;
        size.LowPart = GetFileSize((HANDLE)_get_osfhandle(segment->fd), (LPDWORD)&size.HighPart);
        segment->size = size.QuadPart;

        // 更新 WAL 状态
        if (segment->id >= wal->next_segment_id) {
            wal->next_segment_id = segment->id + 1;
        }
        if (segment->last_sequence >= wal->next_sequence) {
            wal->next_sequence = segment->last_sequence + 1;
        }
        wal->total_size += segment->size;

        // 将段添加到链表
        segment->next = NULL;
        if (!wal->segments) {
            wal->segments = segment;
        } else {
            wal_segment_t* curr = wal->segments;
            while (curr->next) {
                curr = curr->next;
            }
            curr->next = segment;
        }
        wal->segment_count++;

    } while (FindNextFile(find_handle, &find_data));

    FindClose(find_handle);
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

    // 创建新文件
    SECURITY_ATTRIBUTES sa = {
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .lpSecurityDescriptor = NULL,
        .bInheritHandle = FALSE
    };
    HANDLE file_handle = CreateFile(new_segment->filename,
                                  GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ,  // 允许其他进程读取
                                  &sa,
                                  CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING,
                                  NULL);
    if (file_handle == INVALID_HANDLE_VALUE) {
        free(new_segment->filename);
        free(new_segment);
        return PPDB_ERR_IO;
    }

    // 复制文件句柄
    HANDLE dup_handle;
    if (!DuplicateHandle(GetCurrentProcess(), file_handle,
                        GetCurrentProcess(), &dup_handle,
                        GENERIC_READ | GENERIC_WRITE,
                        FALSE,
                        0)) {
        CloseHandle(file_handle);
        free(new_segment->filename);
        free(new_segment);
        return PPDB_ERR_IO;
    }

    // 创建事件对象
    HANDLE event_handle = CreateEvent(&sa, TRUE, FALSE, NULL);
    if (event_handle == NULL) {
        CloseHandle(dup_handle);
        CloseHandle(file_handle);
        free(new_segment->filename);
        free(new_segment);
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
                    CloseHandle(file_handle);
                    free(new_segment->filename);
                    free(new_segment);
                    return PPDB_ERR_TIMEOUT;
                }
                continue;
            } else {
                CloseHandle(event_handle);
                CloseHandle(dup_handle);
                CloseHandle(file_handle);
                free(new_segment->filename);
                free(new_segment);
                return PPDB_ERR_IO;
            }
        } else {
            CloseHandle(event_handle);
            CloseHandle(dup_handle);
            CloseHandle(file_handle);
            free(new_segment->filename);
            free(new_segment);
            return PPDB_ERR_IO;
        }
    }

    // 转换为文件描述符
    new_segment->fd = _open_osfhandle((intptr_t)file_handle, _O_RDWR | _O_BINARY | _O_SEQUENTIAL | _O_DIRECT);
    if (new_segment->fd < 0) {
        UnlockFileEx(dup_handle, 0, MAXDWORD, MAXDWORD, &overlapped);
        CloseHandle(event_handle);
        CloseHandle(dup_handle);
        CloseHandle(file_handle);
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

    // 分配对齐的缓冲区用于头部
    void* aligned_header_buffer = _aligned_malloc(512, 512);
    if (!aligned_header_buffer) {
        UnlockFileEx(dup_handle, 0, MAXDWORD, MAXDWORD, &overlapped);
        CloseHandle(event_handle);
        CloseHandle(dup_handle);
        close(new_segment->fd);
        free(new_segment->filename);
        free(new_segment);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 复制头部到对齐的缓冲区
    memcpy(aligned_header_buffer, &header, sizeof(header));
    memset((char*)aligned_header_buffer + sizeof(header), 0, 512 - sizeof(header));

    // 移动到文件开头
    LARGE_INTEGER offset = {0};
    if (!SetFilePointerEx(dup_handle, offset, NULL, FILE_BEGIN)) {
        _aligned_free(aligned_header_buffer);
        UnlockFileEx(dup_handle, 0, MAXDWORD, MAXDWORD, &overlapped);
        CloseHandle(event_handle);
        CloseHandle(dup_handle);
        close(new_segment->fd);
        free(new_segment->filename);
        free(new_segment);
        return PPDB_ERR_IO;
    }

    // 写入头部
    DWORD bytes_written;
    if (!WriteFile(dup_handle, aligned_header_buffer, 512, &bytes_written, NULL) ||
        bytes_written != 512) {
        _aligned_free(aligned_header_buffer);
        UnlockFileEx(dup_handle, 0, MAXDWORD, MAXDWORD, &overlapped);
        CloseHandle(event_handle);
        CloseHandle(dup_handle);
        close(new_segment->fd);
        free(new_segment->filename);
        free(new_segment);
        return PPDB_ERR_IO;
    }

    _aligned_free(aligned_header_buffer);

    // 确保段头部写入磁盘
    FlushFileBuffers(dup_handle);

    // 初始化段信息
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

    // 更新 WAL 状态
    wal->segment_count++;
    wal->current_fd = new_segment->fd;
    wal->current_size = new_segment->size;

    // 确保目录项写入磁盘
    HANDLE dir_handle = CreateFile(wal->dir_path,
                                 GENERIC_READ,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                 &sa,
                                 OPEN_EXISTING,
                                 FILE_FLAG_BACKUP_SEMANTICS,
                                 NULL);
    if (dir_handle != INVALID_HANDLE_VALUE) {
        FlushFileBuffers(dir_handle);
        CloseHandle(dir_handle);
    }

    // 解锁文件
    UnlockFileEx(dup_handle, 0, MAXDWORD, MAXDWORD, &overlapped);
    CloseHandle(event_handle);
    CloseHandle(dup_handle);

    *segment = new_segment;
    return PPDB_OK;
}

// 封存段
static ppdb_error_t seal_segment(wal_segment_t* segment) {
    if (!segment || segment->is_sealed) return PPDB_OK;
    
    // 获取文件句柄
    HANDLE file_handle = (HANDLE)_get_osfhandle(segment->fd);
    if (file_handle == INVALID_HANDLE_VALUE) {
        return PPDB_ERR_IO;
    }

    // 分配对齐的缓冲区
    void* aligned_buffer = _aligned_malloc(WAL_SEGMENT_HEADER_SIZE, 512);
    if (!aligned_buffer) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 更新段头部
    wal_segment_header_t header;
    DWORD bytes_read;
    LARGE_INTEGER offset;
    offset.QuadPart = 0;

    if (!SetFilePointerEx(file_handle, offset, NULL, FILE_BEGIN) ||
        !ReadFile(file_handle, aligned_buffer, WAL_SEGMENT_HEADER_SIZE, &bytes_read, NULL) ||
        bytes_read != WAL_SEGMENT_HEADER_SIZE) {
        _aligned_free(aligned_buffer);
        return PPDB_ERR_IO;
    }

    // 复制数据到头部结构
    memcpy(&header, aligned_buffer, WAL_SEGMENT_HEADER_SIZE);

    header.last_sequence = segment->last_sequence;
    header.checksum = 0;
    header.checksum = calculate_crc32(&header, WAL_SEGMENT_HEADER_SIZE);

    // 复制回对齐的缓冲区
    memcpy(aligned_buffer, &header, WAL_SEGMENT_HEADER_SIZE);

    DWORD bytes_written;
    offset.QuadPart = 0;
    if (!SetFilePointerEx(file_handle, offset, NULL, FILE_BEGIN) ||
        !WriteFile(file_handle, aligned_buffer, WAL_SEGMENT_HEADER_SIZE, &bytes_written, NULL) ||
        bytes_written != WAL_SEGMENT_HEADER_SIZE) {
        _aligned_free(aligned_buffer);
        return PPDB_ERR_IO;
    }

    _aligned_free(aligned_buffer);

    // 同步文件
    if (!FlushFileBuffers(file_handle)) {
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

    // 保留最新的段
    size_t segments_to_remove = wal->segment_count - wal->config.max_segments;
    wal_segment_t* current = wal->segments;
    wal_segment_t* prev = NULL;

    for (size_t i = 0; i < segments_to_remove && current; i++) {
        wal_segment_t* to_remove = current;
        current = current->next;

        // 关闭并删除文件
        close(to_remove->fd);
        unlink(to_remove->filename);
        free(to_remove->filename);
        free(to_remove);

        wal->segment_count--;
    }

    // 更新段链表头
    wal->segments = current;
    return PPDB_OK;
}

// 创建 WAL
ppdb_error_t ppdb_wal_create_basic(const ppdb_wal_config_t* config, ppdb_wal_t** wal) {
    if (!config || !wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 验证配置
    if (!config->dir_path[0] || 
        config->segment_size < WAL_SEGMENT_HEADER_SIZE || 
        config->max_segments < 1) {
        return PPDB_ERR_INVALID_CONFIG;
    }

    // 分配 WAL 结构
    ppdb_wal_t* new_wal = malloc(sizeof(ppdb_wal_t));
    if (!new_wal) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 初始化基本字段
    new_wal->config = *config;
    new_wal->dir_path = strdup(config->dir_path);
    if (!new_wal->dir_path) {
        free(new_wal);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    new_wal->segments = NULL;
    new_wal->segment_count = 0;
    new_wal->next_sequence = 1;
    new_wal->next_segment_id = 0;
    new_wal->current_fd = -1;
    new_wal->current_size = 0;
    new_wal->total_size = 0;
    new_wal->closed = false;
    new_wal->sync_on_write = config->sync_write;

    // 分配写入缓冲区
    new_wal->write_buffer = malloc(WAL_BUFFER_SIZE);
    if (!new_wal->write_buffer) {
        free(new_wal->dir_path);
        free(new_wal);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 创建同步对象
    ppdb_sync_config_t sync_config = {
        .type = PPDB_SYNC_MUTEX,
        .spin_count = 1000,
        .timeout_ms = 1000,
        .use_lockfree = false,
        .stripe_count = 1,
        .backoff_us = 100,
        .enable_ref_count = false
    };
    new_wal->sync = ppdb_sync_create(&sync_config);
    if (!new_wal->sync) {
        free(new_wal->write_buffer);
        free(new_wal->dir_path);
        free(new_wal);
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // 创建目录
    if (!CreateDirectory(new_wal->dir_path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        ppdb_sync_destroy(new_wal->sync);
        free(new_wal->write_buffer);
        free(new_wal->dir_path);
        free(new_wal);
        return PPDB_ERR_IO;
    }

    // 扫描现有段
    ppdb_error_t err = scan_existing_segments(new_wal);
    if (err != PPDB_OK) {
        ppdb_sync_destroy(new_wal->sync);
        free(new_wal->write_buffer);
        free(new_wal->dir_path);
        free(new_wal);
        return err;
    }

    // 创建新段
    wal_segment_t* first_segment;
    err = create_new_segment(new_wal, &first_segment);
    if (err != PPDB_OK) {
        ppdb_sync_destroy(new_wal->sync);
        free(new_wal->write_buffer);
        free(new_wal->dir_path);
        free(new_wal);
        return err;
    }

    *wal = new_wal;
    return PPDB_OK;
}

// 销毁 WAL
void ppdb_wal_destroy_basic(ppdb_wal_t* wal) {
    if (!wal) return;

    // 关闭所有段
    wal_segment_t* current = wal->segments;
    while (current) {
        wal_segment_t* next = current->next;
        close(current->fd);
        free(current->filename);
        free(current);
        current = next;
    }

    // 释放资源
    ppdb_sync_destroy(wal->sync);
    free(wal->dir_path);
    free(wal);
}

// 写入记录到段
static ppdb_error_t write_record_to_segment(wal_segment_t* segment, const void* key, size_t key_size,
                                          const void* value, size_t value_size, uint64_t sequence) {
    if (!segment || !key || !value) {
        return PPDB_ERR_INVALID_ARG;
    }

    // 准备记录头部
    wal_record_header_t header = {
        .magic = WAL_MAGIC,
        .type = PPDB_WAL_RECORD_PUT,
        .key_size = key_size,
        .value_size = value_size,
        .sequence = sequence,
        .checksum = 0
    };

    // 计算校验和
    header.checksum = calculate_crc32(&header, sizeof(header));
    header.checksum = calculate_crc32(key, key_size);
    header.checksum = calculate_crc32(value, value_size);

    // 写入头部
    ssize_t write_size = write(segment->fd, &header, sizeof(header));
    if (write_size != sizeof(header)) {
        return PPDB_ERR_IO;
    }

    // 写入键
    write_size = write(segment->fd, key, key_size);
    if (write_size != key_size) {
        return PPDB_ERR_IO;
    }

    // 写入值
    write_size = write(segment->fd, value, value_size);
    if (write_size != value_size) {
        return PPDB_ERR_IO;
    }

    // 更新段信息
    segment->size += sizeof(header) + key_size + value_size;
    segment->last_sequence = sequence;

    return PPDB_OK;
}

// 同步 WAL
ppdb_error_t ppdb_wal_sync_basic(ppdb_wal_t* wal) {
    if (!wal) {
        return PPDB_ERR_INVALID_ARG;
    }

    if (wal->closed) {
        return PPDB_ERR_WAL_CLOSED;
    }

    // 同步当前段
    if (fsync(wal->current_fd) != 0) {
        return PPDB_ERR_IO;
    }

    return PPDB_OK;
}

// 获取 WAL 大小
size_t ppdb_wal_size_basic(ppdb_wal_t* wal) {
    if (!wal) {
        return 0;
    }

    return wal->total_size;
}

// 获取下一个序列号
uint64_t ppdb_wal_next_sequence_basic(ppdb_wal_t* wal) {
    if (!wal) {
        return 0;
    }

    return wal->next_sequence;
}

// 同步 WAL
ppdb_error_t ppdb_wal_sync(ppdb_wal_t* wal) {
    return ppdb_wal_sync_basic(wal);
}

// 获取 WAL 大小
size_t ppdb_wal_size(ppdb_wal_t* wal) {
    return ppdb_wal_size_basic(wal);
}

// 获取下一个序列号
uint64_t ppdb_wal_next_sequence(ppdb_wal_t* wal) {
    return ppdb_wal_next_sequence_basic(wal);
}

// 关闭 WAL
void ppdb_wal_close(ppdb_wal_t* wal) {
    if (!wal || wal->closed) {
        return;
    }

    // 关闭所有段
    wal_segment_t* current = wal->segments;
    while (current) {
        close(current->fd);
        current = current->next;
    }

    wal->closed = true;
}

// 获取段数量
size_t ppdb_wal_segment_count(ppdb_wal_t* wal) {
    if (!wal) {
        return 0;
    }
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
