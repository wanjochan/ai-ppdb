#include <cosmopolitan.h>
#include "internal/kvstore_fs.h"

// 写入文件
ppdb_error_t ppdb_write_file(const char* filename, const void* data, size_t size) {
    if (!filename || !data) {
        return PPDB_ERR_INVALID_ARG;
    }

    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        return PPDB_ERR_IO;
    }

    size_t written = fwrite(data, 1, size, fp);
    if (written != size) {
        fclose(fp);
        return PPDB_ERR_IO;
    }

    if (fclose(fp) != 0) {
        return PPDB_ERR_IO;
    }

    return PPDB_OK;
}

// 读取文件
ppdb_error_t ppdb_read_file(const char* filename, void* data, size_t size) {
    if (!filename || !data) {
        return PPDB_ERR_INVALID_ARG;
    }

    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        return PPDB_ERR_IO;
    }

    size_t read = fread(data, 1, size, fp);
    if (read != size) {
        fclose(fp);
        return PPDB_ERR_IO;
    }

    if (fclose(fp) != 0) {
        return PPDB_ERR_IO;
    }

    return PPDB_OK;
}

// 追加到文件
ppdb_error_t ppdb_append_file(const char* filename, const void* data, size_t size) {
    if (!filename || !data) {
        return PPDB_ERR_INVALID_ARG;
    }

    FILE* fp = fopen(filename, "ab");
    if (!fp) {
        return PPDB_ERR_IO;
    }

    size_t written = fwrite(data, 1, size, fp);
    if (written != size) {
        fclose(fp);
        return PPDB_ERR_IO;
    }

    if (fclose(fp) != 0) {
        return PPDB_ERR_IO;
    }

    return PPDB_OK;
}

// 获取文件大小
ppdb_error_t ppdb_get_file_size(const char* filename, size_t* size) {
    if (!filename || !size) {
        return PPDB_ERR_INVALID_ARG;
    }

    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        return PPDB_ERR_IO;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return PPDB_ERR_IO;
    }

    long file_size = ftell(fp);
    if (file_size < 0) {
        fclose(fp);
        return PPDB_ERR_IO;
    }

    *size = (size_t)file_size;

    if (fclose(fp) != 0) {
        return PPDB_ERR_IO;
    }

    return PPDB_OK;
} 