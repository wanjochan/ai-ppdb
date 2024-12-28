#include <cosmopolitan.h>
#include "ppdb/fs.h"

ppdb_error_t ppdb_ensure_directory(const char* path) {
    if (!path) return PPDB_ERR_INVALID_ARG;

    struct stat st;
    if (stat(path, &st) == 0) 
        return S_ISDIR(st.st_mode) ? PPDB_OK : PPDB_ERR_EXISTS;

    return mkdir(path, 0755) == 0 ? PPDB_OK : PPDB_ERR_IO;
}

static ppdb_error_t handle_path_entry(const char* base_path, const char* entry_name) {
    char full_path[PATH_MAX];
    size_t total_len = snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry_name);
    if (total_len >= sizeof(full_path)) return PPDB_ERR_INVALID_ARG;

    struct stat st;
    if (stat(full_path, &st) != 0) return PPDB_ERR_IO;

    if (S_ISDIR(st.st_mode)) {
        ppdb_error_t err = ppdb_remove_directory(full_path);
        if (err != PPDB_OK) return err;
    } else if (unlink(full_path) != 0) {
        return PPDB_ERR_IO;
    }
    return PPDB_OK;
}

ppdb_error_t ppdb_remove_directory(const char* path) {
    if (!path) return PPDB_ERR_INVALID_ARG;

    struct stat st;
    if (stat(path, &st) != 0) 
        return errno == ENOENT ? PPDB_OK : PPDB_ERR_IO;

    if (!S_ISDIR(st.st_mode)) return PPDB_ERR_NOT_SUPPORTED;

    DIR* dir = opendir(path);
    if (!dir) return PPDB_ERR_IO;

    struct dirent* entry;
    ppdb_error_t err = PPDB_OK;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) 
            continue;

        err = handle_path_entry(path, entry->d_name);
        if (err != PPDB_OK) break;
    }

    closedir(dir);
    return err == PPDB_OK ? (rmdir(path) == 0 ? PPDB_OK : PPDB_ERR_IO) : err;
} 