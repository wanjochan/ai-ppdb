//-----------------------------------------------------------------------------
// File System Operations Implementation
//-----------------------------------------------------------------------------

struct ppdb_core_file {
    FILE* fp;
    char* path;
    char* mode;
};

ppdb_error_t ppdb_core_file_open(const char* path, const char* mode, ppdb_core_file_t** file) {
    if (!path || !mode || !file) return PPDB_ERR_NULL_POINTER;
    
    *file = ppdb_core_alloc(sizeof(ppdb_core_file_t));
    if (!*file) return PPDB_ERR_OUT_OF_MEMORY;
    
    (*file)->path = strdup(path);
    (*file)->mode = strdup(mode);
    if (!(*file)->path || !(*file)->mode) {
        ppdb_core_free((*file)->path);
        ppdb_core_free((*file)->mode);
        ppdb_core_free(*file);
        *file = NULL;
        return PPDB_ERR_OUT_OF_MEMORY;
    }
    
    (*file)->fp = fopen(path, mode);
    if (!(*file)->fp) {
        ppdb_core_free((*file)->path);
        ppdb_core_free((*file)->mode);
        ppdb_core_free(*file);
        *file = NULL;
        return PPDB_ERR_IO;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_core_file_close(ppdb_core_file_t* file) {
    if (!file) return PPDB_ERR_NULL_POINTER;
    
    if (fclose(file->fp) != 0) {
        return PPDB_ERR_IO;
    }
    
    ppdb_core_free(file->path);
    ppdb_core_free(file->mode);
    ppdb_core_free(file);
    return PPDB_OK;
}

ppdb_error_t ppdb_core_file_read(ppdb_core_file_t* file, void* buf, size_t size, size_t* read) {
    if (!file || !buf || !read) return PPDB_ERR_NULL_POINTER;
    
    *read = fread(buf, 1, size, file->fp);
    if (*read != size && ferror(file->fp)) {
        return PPDB_ERR_IO;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_core_file_write(ppdb_core_file_t* file, const void* buf, size_t size, size_t* written) {
    if (!file || !buf || !written) return PPDB_ERR_NULL_POINTER;
    
    *written = fwrite(buf, 1, size, file->fp);
    if (*written != size) {
        return PPDB_ERR_IO;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_core_file_sync(ppdb_core_file_t* file) {
    if (!file) return PPDB_ERR_NULL_POINTER;
    
    if (fflush(file->fp) != 0) {
        return PPDB_ERR_IO;
    }
    
    #ifdef _WIN32
    if (_commit(_fileno(file->fp)) != 0) {
        return PPDB_ERR_IO;
    }
    #else
    if (fsync(fileno(file->fp)) != 0) {
        return PPDB_ERR_IO;
    }
    #endif
    
    return PPDB_OK;
}
