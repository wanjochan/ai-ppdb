#include "../internal/peer.h"
#include "../internal/storage.h"
#include <cosmopolitan.h>

// Create connection
ppdb_error_t ppdb_conn_create(ppdb_handle_t* conn, const peer_ops_t* ops, void* user_data) {
    PPDB_UNUSED(user_data);

    if (!conn || !ops) {
        return PPDB_ERR_PARAM;
    }

    ppdb_conn_state_t* state = calloc(1, sizeof(ppdb_conn_state_t));
    if (!state) {
        return PPDB_ERR_MEMORY;
    }

    // Initialize protocol instance
    ppdb_error_t err = ops->create(&state->proto, user_data);
    if (err != PPDB_OK) {
        free(state);
        return err;
    }

    state->ops = ops;
    state->user_data = user_data;
    *conn = (ppdb_handle_t)state;
    return PPDB_OK;
}

// Destroy connection
void ppdb_conn_destroy(ppdb_handle_t conn) {
    ppdb_conn_state_t* state = (ppdb_conn_state_t*)conn;
    if (!state) {
        return;
    }

    if (state->proto && state->ops) {
        state->ops->destroy(state->proto);
    }

    if (state->connected) {
        close(state->fd);
    }

    free(state);
}

// Set connection socket
ppdb_error_t ppdb_conn_set_socket(ppdb_handle_t conn, int fd) {
    ppdb_conn_state_t* state = (ppdb_conn_state_t*)conn;
    if (!state) {
        return PPDB_ERR_PARAM;
    }

    state->fd = fd;
    state->connected = true;

    // Notify protocol
    return state->ops->on_connect(state->proto, conn);
}

// Close connection
void ppdb_conn_close(ppdb_handle_t conn) {
    ppdb_conn_state_t* state = (ppdb_conn_state_t*)conn;
    if (!state || !state->connected) {
        return;
    }

    // Notify protocol
    state->ops->on_disconnect(state->proto, conn);

    close(state->fd);
    state->fd = -1;
    state->connected = false;
}

// Send data
ppdb_error_t ppdb_conn_send(ppdb_handle_t conn, const void* data, size_t size) {
    ppdb_conn_state_t* state = (ppdb_conn_state_t*)conn;
    if (!state || !state->connected) {
        return PPDB_ERR_INVALID_STATE;
    }

    // Write to socket
    ssize_t written = write(state->fd, data, size);
    if (written < 0) {
        return PPDB_ERR_IO;
    }
    if ((size_t)written < size) {
        return PPDB_ERR_PARTIAL_WRITE;
    }

    return PPDB_OK;
}

// Write data
ppdb_error_t ppdb_conn_write(ppdb_handle_t conn, const uint8_t* data, size_t size) {
    ppdb_conn_state_t* state = (ppdb_conn_state_t*)conn;
    if (!state || !state->connected) {
        return PPDB_ERR_INVALID_STATE;
    }

    // Write to socket
    ssize_t written = write(state->fd, data, size);
    if (written < 0) {
        return PPDB_ERR_IO;
    }
    if ((size_t)written < size) {
        return PPDB_ERR_PARTIAL_WRITE;
    }

    return PPDB_OK;
}

// Receive data
ppdb_error_t ppdb_conn_recv(ppdb_handle_t conn, void* data, size_t size) {
    ppdb_conn_state_t* state = (ppdb_conn_state_t*)conn;
    if (!state || !state->connected) {
        return PPDB_ERR_INVALID_STATE;
    }

    // Read from socket
    ssize_t nread = read(state->fd, data, size);
    if (nread < 0) {
        return PPDB_ERR_IO;
    }
    if (nread == 0) {
        return PPDB_ERR_CONNECTION_CLOSED;
    }
    if ((size_t)nread < size) {
        return PPDB_ERR_PARTIAL_READ;
    }

    // Process data through protocol
    return state->ops->on_data(state->proto, conn, data, nread);
}

// Get connection state
bool ppdb_conn_is_connected(ppdb_handle_t conn) {
    ppdb_conn_state_t* state = (ppdb_conn_state_t*)conn;
    return state && state->connected;
}

// Get protocol name
const char* ppdb_conn_get_proto_name(ppdb_handle_t conn) {
    ppdb_conn_state_t* state = (ppdb_conn_state_t*)conn;
    if (!state || !state->proto || !state->ops) {
        return NULL;
    }
    return state->ops->get_name(state->proto);
} 