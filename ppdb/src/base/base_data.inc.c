//-----------------------------------------------------------------------------
// Data Management Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_data_create(const void* data, size_t size, ppdb_data_t* out) {
    if (!data || !out) return PPDB_ERR_NULL_POINTER;
    if (size == 0) return PPDB_ERR_INVALID_SIZE;
    
    memset(out, 0, sizeof(ppdb_data_t));
    out->size = size;
    
    // Small data optimization
    if (size <= sizeof(out->inline_data)) {
        memcpy(out->inline_data, data, size);
        out->flags = 1;  // Using inline storage
        return PPDB_OK;
    }
    
    // Allocate extended storage
    out->extended_data = ppdb_core_alloc(size);
    if (!out->extended_data) return PPDB_ERR_OUT_OF_MEMORY;
    
    memcpy(out->extended_data, data, size);
    out->flags = 2;  // Using extended storage
    return PPDB_OK;
}

ppdb_error_t ppdb_data_destroy(ppdb_data_t* data) {
    if (!data) return PPDB_ERR_NULL_POINTER;
    
    if (data->flags == 2 && data->extended_data) {
        ppdb_core_free(data->extended_data);
    }
    
    memset(data, 0, sizeof(ppdb_data_t));
    return PPDB_OK;
}

ppdb_error_t ppdb_data_get(const ppdb_data_t* data, void* buf, size_t size, size_t* copied) {
    if (!data || !buf || !copied) return PPDB_ERR_NULL_POINTER;
    if (size == 0) return PPDB_ERR_INVALID_SIZE;
    
    *copied = (size < data->size) ? size : data->size;
    
    if (data->flags == 1) {
        // Copy from inline storage
        memcpy(buf, data->inline_data, *copied);
    } else if (data->flags == 2 && data->extended_data) {
        // Copy from extended storage
        memcpy(buf, data->extended_data, *copied);
    } else {
        return PPDB_ERR_INVALID_STATE;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_data_size(const ppdb_data_t* data, size_t* size) {
    if (!data || !size) return PPDB_ERR_NULL_POINTER;
    
    *size = data->size;
    return PPDB_OK;
}
