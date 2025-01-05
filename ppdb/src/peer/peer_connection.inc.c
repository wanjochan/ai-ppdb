#include "ppdb/internal.h"

// Implementation of connection functions
ppdb_error_t ppdb_peer_connection_create(ppdb_engine_async_loop_t* loop, ppdb_peer_connection_t** conn) {
    ppdb_error_t err;
    ppdb_peer_connection_t* c;

    if (!loop || !conn) {
        return PPDB_ERR_NULL_POINTER;
    }

    // Allocate connection structure
    c = ppdb_engine_malloc(sizeof(ppdb_peer_connection_t));
    if (!c) {
        return PPDB_ERR_OUT_OF_MEMORY;
    }

    // Initialize fields
    c->loop = loop;
    c->connected = false;
    c->retry_count = 0;

    // Create mutex
    err = ppdb_engine_mutex_create(&c->mutex);
    if (err != PPDB_OK) {
        ppdb_engine_free(c);
        return err;
    }

    // Create async handle
    err = ppdb_engine_async_handle_create(loop, &c->handle);
    if (err != PPDB_OK) {
        ppdb_engine_mutex_destroy(c->mutex);
        ppdb_engine_free(c);
        return err;
    }

    *conn = c;
    return PPDB_OK;
}

void ppdb_peer_connection_destroy(ppdb_peer_connection_t* conn) {
    if (!conn) {
        return;
    }

    // Cleanup async handle
    if (conn->handle) {
        ppdb_engine_async_handle_destroy(conn->handle);
    }

    // Cleanup mutex
    if (conn->mutex) {
        ppdb_engine_mutex_destroy(conn->mutex);
    }

    ppdb_engine_free(conn);
}

static void connection_callback(ppdb_engine_async_handle_t* handle, ppdb_error_t status) {
    ppdb_peer_connection_t* conn = handle->data;
    
    if (status == PPDB_OK) {
        conn->connected = true;
        conn->retry_count = 0;
    } else {
        conn->connected = false;
        conn->retry_count++;
    }
}

ppdb_error_t ppdb_peer_connection_connect(ppdb_peer_connection_t* conn, const char* host, uint16_t port) {
    ppdb_error_t err;

    if (!conn || !host) {
        return PPDB_ERR_NULL_POINTER;
    }

    err = ppdb_engine_mutex_lock(conn->mutex);
    if (err != PPDB_OK) {
        return err;
    }

    if (conn->connected) {
        ppdb_engine_mutex_unlock(conn->mutex);
        return PPDB_ERR_INVALID_STATE;
    }

    // TODO: Implement actual network connection
    // For now, just simulate success
    conn->connected = true;
    conn->retry_count = 0;

    ppdb_engine_mutex_unlock(conn->mutex);
    return PPDB_OK;
}

ppdb_error_t ppdb_peer_connection_disconnect(ppdb_peer_connection_t* conn) {
    ppdb_error_t err;

    if (!conn) {
        return PPDB_ERR_NULL_POINTER;
    }

    err = ppdb_engine_mutex_lock(conn->mutex);
    if (err != PPDB_OK) {
        return err;
    }

    if (!conn->connected) {
        ppdb_engine_mutex_unlock(conn->mutex);
        return PPDB_ERR_INVALID_STATE;
    }

    // TODO: Implement actual network disconnection
    conn->connected = false;

    ppdb_engine_mutex_unlock(conn->mutex);
    return PPDB_OK;
}

ppdb_error_t ppdb_peer_msg_send(ppdb_peer_connection_t* conn, ppdb_peer_msg_type_t type, const void* payload, size_t size) {
    ppdb_error_t err;
    ppdb_peer_msg_header_t header;

    if (!conn || (!payload && size > 0)) {
        return PPDB_ERR_NULL_POINTER;
    }

    err = ppdb_engine_mutex_lock(conn->mutex);
    if (err != PPDB_OK) {
        return err;
    }

    if (!conn->connected) {
        ppdb_engine_mutex_unlock(conn->mutex);
        return PPDB_ERR_INVALID_STATE;
    }

    // Prepare header
    header.magic = 0x50504442;  // "PPDB"
    header.version = 1;
    header.type = type;
    header.payload_size = size;

    // Send header
    err = ppdb_engine_async_write(conn->handle, &header, sizeof(header), NULL);
    if (err != PPDB_OK) {
        ppdb_engine_mutex_unlock(conn->mutex);
        return err;
    }

    // Send payload if any
    if (payload && size > 0) {
        err = ppdb_engine_async_write(conn->handle, payload, size, NULL);
        if (err != PPDB_OK) {
            ppdb_engine_mutex_unlock(conn->mutex);
            return err;
        }
    }

    ppdb_engine_mutex_unlock(conn->mutex);
    return PPDB_OK;
}

ppdb_error_t ppdb_peer_msg_recv(ppdb_peer_connection_t* conn, ppdb_peer_msg_header_t* header, void* payload, size_t* size) {
    ppdb_error_t err;
    size_t read_size;

    if (!conn || !header || !payload || !size) {
        return PPDB_ERR_NULL_POINTER;
    }

    err = ppdb_engine_mutex_lock(conn->mutex);
    if (err != PPDB_OK) {
        return err;
    }

    if (!conn->connected) {
        ppdb_engine_mutex_unlock(conn->mutex);
        return PPDB_ERR_INVALID_STATE;
    }

    // Read header
    err = ppdb_engine_async_read(conn->handle, header, sizeof(*header), NULL);
    if (err != PPDB_OK) {
        ppdb_engine_mutex_unlock(conn->mutex);
        return err;
    }

    // Verify header
    if (header->magic != 0x50504442) {
        ppdb_engine_mutex_unlock(conn->mutex);
        return PPDB_ERR_INVALID_STATE;
    }

    if (header->payload_size > *size) {
        ppdb_engine_mutex_unlock(conn->mutex);
        return PPDB_ERR_INVALID_SIZE;
    }

    // Read payload if any
    if (header->payload_size > 0) {
        err = ppdb_engine_async_read(conn->handle, payload, header->payload_size, NULL);
        if (err != PPDB_OK) {
            ppdb_engine_mutex_unlock(conn->mutex);
            return err;
        }
    }

    *size = header->payload_size;
    ppdb_engine_mutex_unlock(conn->mutex);
    return PPDB_OK;
}
