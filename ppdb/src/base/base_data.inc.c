#ifndef PPDB_BASE_DATA_INC_C
#define PPDB_BASE_DATA_INC_C

ppdb_error_t ppdb_data_create(ppdb_data_t* data, const void* src, size_t size) {
    if (!data) return PPDB_ERR_NULL_POINTER;
    if (size == 0) return PPDB_ERR_INVALID_ARGUMENT;

    memset(data, 0, sizeof(ppdb_data_t));
    data->size = size;

    if (size <= sizeof(data->inline_data)) {
        // Use inline storage for small data
        if (src) {
            memcpy(data->inline_data, src, size);
        }
        data->flags = 0;
        data->extended_data = NULL;
    } else {
        // Allocate extended storage for large data
        data->extended_data = ppdb_aligned_alloc(size);
        if (!data->extended_data) return PPDB_ERR_OUT_OF_MEMORY;

        if (src) {
            memcpy(data->extended_data, src, size);
        }
        data->flags = 1;  // Using extended storage
    }

    return PPDB_OK;
}

void ppdb_data_destroy(ppdb_data_t* data) {
    if (!data) return;

    if (data->flags & 1) {  // Using extended storage
        ppdb_aligned_free(data->extended_data);
    }

    memset(data, 0, sizeof(ppdb_data_t));
}

ppdb_error_t ppdb_data_get(const ppdb_data_t* data, void* dst, size_t size) {
    if (!data || !dst) return PPDB_ERR_NULL_POINTER;
    if (size == 0) return PPDB_ERR_INVALID_ARGUMENT;
    if (size < data->size) return PPDB_ERR_INVALID_ARGUMENT;

    if (data->flags & 1) {  // Using extended storage
        memcpy(dst, data->extended_data, data->size);
    } else {
        memcpy(dst, data->inline_data, data->size);
    }

    return PPDB_OK;
}

#endif // PPDB_BASE_DATA_INC_C
