#ifndef PPDB_BASE_DATA_INC_C
#define PPDB_BASE_DATA_INC_C

#define DATA_ALIGNMENT 16  // 添加对齐常量

ppdb_error_t ppdb_data_create(ppdb_data_t* data, const void* src, size_t size) {
    if (!data || (!src && size > 0)) return PPDB_ERR_NULL_POINTER;

    // Clear data structure
    memset(data, 0, sizeof(ppdb_data_t));
    data->size = size;

    // Handle empty data
    if (size == 0) return PPDB_OK;

    // Use inline storage if possible
    if (size <= sizeof(data->inline_data)) {
        memcpy(data->inline_data, src, size);
    } else {
        // Allocate extended storage
        data->extended_data = ppdb_aligned_alloc(DATA_ALIGNMENT, size);
        if (!data->extended_data) return PPDB_ERR_OUT_OF_MEMORY;
        memcpy(data->extended_data, src, size);
        data->flags |= 1;  // Mark as extended
    }

    return PPDB_OK;
}

void ppdb_data_destroy(ppdb_data_t* data) {
    if (!data) return;
    if (data->flags & 1) {  // Extended storage
        ppdb_aligned_free(data->extended_data);
    }
    memset(data, 0, sizeof(ppdb_data_t));
}

ppdb_error_t ppdb_data_copy(ppdb_data_t* dst, const ppdb_data_t* src) {
    if (!dst || !src) return PPDB_ERR_NULL_POINTER;
    if (src->flags & 1) {
        return ppdb_data_create(dst, src->extended_data, src->size);
    } else {
        return ppdb_data_create(dst, src->inline_data, src->size);
    }
}

#endif // PPDB_BASE_DATA_INC_C
