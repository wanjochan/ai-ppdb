#include "internal/infra/infra.h"

int infra_buffer_init(struct infra_buffer* buf, size_t initial_capacity) {
    buf->data = infra_malloc(initial_capacity);
    if (!buf->data) {
        return INFRA_ERR_NOMEM;
    }
    
    buf->size = 0;
    buf->capacity = initial_capacity;
    buf->read_pos = 0;
    buf->write_pos = 0;
    
    return INFRA_OK;
}

void infra_buffer_destroy(struct infra_buffer* buf) {
    if (buf->data) {
        infra_free(buf->data);
        buf->data = NULL;
    }
    buf->size = 0;
    buf->capacity = 0;
    buf->read_pos = 0;
    buf->write_pos = 0;
}

int infra_buffer_reserve(struct infra_buffer* buf, size_t size) {
    void* new_data;
    size_t new_capacity;
    
    if (size <= buf->capacity) {
        return INFRA_OK;
    }
    
    /* Double the capacity until it's enough */
    new_capacity = buf->capacity;
    while (new_capacity < size) {
        new_capacity *= 2;
    }
    
    new_data = infra_realloc(buf->data, new_capacity);
    if (!new_data) {
        return INFRA_ERR_NOMEM;
    }
    
    buf->data = new_data;
    buf->capacity = new_capacity;
    
    return INFRA_OK;
}

int infra_buffer_write(struct infra_buffer* buf, const void* data, size_t size) {
    int ret;
    
    /* Make sure we have enough space */
    ret = infra_buffer_reserve(buf, buf->write_pos + size);
    if (ret != INFRA_OK) {
        return ret;
    }
    
    /* Copy data */
    memcpy((char*)buf->data + buf->write_pos, data, size);
    buf->write_pos += size;
    buf->size = buf->write_pos;
    
    return INFRA_OK;
}

int infra_buffer_read(struct infra_buffer* buf, void* data, size_t size) {
    size_t available = infra_buffer_readable(buf);
    
    if (size > available) {
        return INFRA_ERR_INVALID;
    }
    
    /* Copy data */
    memcpy(data, (char*)buf->data + buf->read_pos, size);
    buf->read_pos += size;
    
    /* Reset positions if buffer is empty */
    if (buf->read_pos == buf->write_pos) {
        buf->read_pos = 0;
        buf->write_pos = 0;
    }
    
    return INFRA_OK;
}

size_t infra_buffer_readable(struct infra_buffer* buf) {
    return buf->write_pos - buf->read_pos;
}

size_t infra_buffer_writable(struct infra_buffer* buf) {
    return buf->capacity - buf->write_pos;
}

void infra_buffer_reset(struct infra_buffer* buf) {
    buf->read_pos = 0;
    buf->write_pos = 0;
    buf->size = 0;
}
