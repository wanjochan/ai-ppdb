#include "peer.h"

// Connection state
typedef struct {
    void* proto;              // Protocol instance
    const peer_ops_t* ops;    // Protocol operations
    void* user_data;          // User data
    bool connected;           // Connection status
    int fd;                   // Socket file descriptor
} ppdb_conn_state_t;

// Connection handle
struct ppdb_conn_s {
    ppdb_conn_state_t state;  // Connection state
    char read_buf[4096];      // Read buffer
    size_t read_pos;          // Read position
    char write_buf[4096];     // Write buffer
    size_t write_pos;         // Write position
};

// Create connection
ppdb_error_t ppdb_conn_create(ppdb_conn_t* conn, const peer_ops_t* ops, void* user_data) {
    if (!conn || !ops) {
        return PPDB_ERR_PARAM;
    }

    *conn = calloc(1, sizeof(struct ppdb_conn_s));
    if (!*conn) {
        return PPDB_ERR_MEMORY;
    }

    // Initialize protocol instance
    ppdb_error_t err = ops->create(&(*conn)->state.proto, user_data);
    if (err != PPDB_OK) {
        free(*conn);
        return err;
    }

    (*conn)->state.ops = ops;
    (*conn)->state.user_data = user_data;
    return PPDB_OK;
}

// Destroy connection
void ppdb_conn_destroy(ppdb_conn_t conn) {
    if (!conn) {
        return;
    }

    if (conn->state.proto && conn->state.ops) {
        conn->state.ops->destroy(conn->state.proto);
    }

    if (conn->state.connected) {
        close(conn->state.fd);
    }

    free(conn);
}

// Set connection socket
ppdb_error_t ppdb_conn_set_socket(ppdb_conn_t conn, int fd) {
    if (!conn) {
        return PPDB_ERR_PARAM;
    }

    conn->state.fd = fd;
    conn->state.connected = true;

    // Notify protocol
    return conn->state.ops->on_connect(conn->state.proto, conn);
}

// Close connection
void ppdb_conn_close(ppdb_conn_t conn) {
    if (!conn || !conn->state.connected) {
        return;
    }

    // Notify protocol
    conn->state.ops->on_disconnect(conn->state.proto, conn);

    close(conn->state.fd);
    conn->state.fd = -1;
    conn->state.connected = false;
}

// Send data
ppdb_error_t ppdb_conn_send(ppdb_conn_t conn, const void* data, size_t size) {
    if (!conn || !conn->state.connected) {
        return PPDB_ERR_INVALID_STATE;
    }

    // Write to socket
    ssize_t written = write(conn->state.fd, data, size);
    if (written < 0) {
        return PPDB_ERR_IO;
    }
    if ((size_t)written < size) {
        return PPDB_ERR_PARTIAL_WRITE;
    }

    return PPDB_OK;
}

// Receive data
ppdb_error_t ppdb_conn_recv(ppdb_conn_t conn, void* data, size_t size) {
    if (!conn || !conn->state.connected) {
        return PPDB_ERR_INVALID_STATE;
    }

    // Read from socket
    ssize_t nread = read(conn->state.fd, data, size);
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
    return conn->state.ops->on_data(conn->state.proto, conn, data, nread);
}

// Get connection state
bool ppdb_conn_is_connected(ppdb_conn_t conn) {
    return conn && conn->state.connected;
}

// Get protocol name
const char* ppdb_conn_get_proto_name(ppdb_conn_t conn) {
    if (!conn || !conn->state.proto || !conn->state.ops) {
        return NULL;
    }
    return conn->state.ops->get_name(conn->state.proto);
} 