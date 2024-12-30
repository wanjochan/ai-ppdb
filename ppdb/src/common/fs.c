#include <cosmopolitan.h>
#include "ppdb/ppdb_error.h"
#include "ppdb/ppdb_fs.h"
#include "ppdb/ppdb_types.h"
#include "kvstore/internal/kvstore_logger.h"

// 目录操作
bool ppdb_fs_dir_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

ppdb_error_t ppdb_fs_mkdir(const char* path) {
    if (!path) {
        return PPDB_ERR_INVALID_ARG;
    }
    
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        return PPDB_ERR_IO;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_fs_rmdir(const char* path) {
    if (!path) {
        return PPDB_ERR_INVALID_ARG;
    }
    
    if (rmdir(path) != 0) {
        return PPDB_ERR_IO;
    }
    return PPDB_OK;
}

// 文件操作
bool ppdb_fs_file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

ppdb_error_t ppdb_fs_remove(const char* path) {
    if (!path) {
        return PPDB_ERR_INVALID_ARG;
    }
    
    if (unlink(path) != 0) {
        return PPDB_ERR_IO;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_fs_rename(const char* old_path, const char* new_path) {
    if (!old_path || !new_path) {
        return PPDB_ERR_INVALID_ARG;
    }
    
    if (rename(old_path, new_path) != 0) {
        return PPDB_ERR_IO;
    }
    return PPDB_OK;
}

// 读写操作
ppdb_error_t ppdb_fs_write(const char* filename, const void* data, size_t size) {
    if (!filename || !data) {
        return PPDB_ERR_INVALID_ARG;
    }

    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        return PPDB_ERR_IO;
    }

    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);

    if (written != size) {
        return PPDB_ERR_IO;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_fs_read(const char* filename, void* data, size_t size, size_t* bytes_read) {
    if (!filename || !data || !bytes_read) {
        return PPDB_ERR_INVALID_ARG;
    }

    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        return PPDB_ERR_IO;
    }

    *bytes_read = fread(data, 1, size, fp);
    fclose(fp);

    if (*bytes_read == 0 && ferror(fp)) {
        return PPDB_ERR_IO;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_fs_append(const char* filename, const void* data, size_t size) {
    if (!filename || !data) {
        return PPDB_ERR_INVALID_ARG;
    }

    FILE* fp = fopen(filename, "ab");
    if (!fp) {
        return PPDB_ERR_IO;
    }

    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);

    if (written != size) {
        return PPDB_ERR_IO;
    }
    return PPDB_OK;
}

// 文件信息
ppdb_error_t ppdb_fs_size(const char* filename, size_t* size) {
    if (!filename || !size) {
        return PPDB_ERR_INVALID_ARG;
    }

    struct stat st;
    if (stat(filename, &st) != 0) {
        return PPDB_ERR_IO;
    }

    *size = st.st_size;
    return PPDB_OK;
}

ppdb_error_t ppdb_fs_sync(const char* filename) {
    if (!filename) {
        return PPDB_ERR_INVALID_ARG;
    }

    FILE* fp = fopen(filename, "r+b");
    if (!fp) {
        return PPDB_ERR_IO;
    }

    if (fsync(fileno(fp)) != 0) {
        fclose(fp);
        return PPDB_ERR_IO;
    }

    fclose(fp);
    return PPDB_OK;
}
