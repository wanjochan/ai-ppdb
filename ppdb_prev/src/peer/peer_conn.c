#include <cosmopolitan.h>
#include "../internal/peer.h"
#include "../internal/database.h"

// Connection state
typedef struct ppdb_conn_state {
    ppdb_ctx_t* ctx;                // Database context
    ppdb_database_txn_t* txn;       // Current transaction
    ppdb_database_table_t* table;   // Current table
    int socket;                     // Socket descriptor
    void* proto_data;               // Protocol-specific data
    bool connected;                 // Connection status
} ppdb_conn_state_t;

// Create connection
ppdb_error_t ppdb_conn_create(ppdb_handle_t* conn, const peer_ops_t* ops, ppdb_ctx_t* ctx) {
    if (!conn || !ops || !ctx) {
        return PPDB_ERR_PARAM;
    }

    // Allocate connection state
    ppdb_conn_state_t* state = calloc(1, sizeof(ppdb_conn_state_t));
    if (!state) {
        return PPDB_ERR_MEMORY;
    }

    // Initialize state
    state->ctx = ctx;
    state->connected = false;
    state->socket = -1;

    // Create transaction
    ppdb_error_t err = ppdb_database_txn_begin(ctx->db, NULL, 0, &state->txn);
    if (err != PPDB_OK) {
        free(state);
        return err;
    }

    // Create handle
    *conn = calloc(1, sizeof(struct ppdb_handle));
    if (!*conn) {
        ppdb_database_txn_abort(state->txn);
        free(state);
        return PPDB_ERR_MEMORY;
    }

    // Initialize handle
    (*conn)->ctx = ctx;
    (*conn)->state = state;
    (*conn)->txn = state->txn;

    return PPDB_OK;
}

// Destroy connection
void ppdb_conn_destroy(ppdb_handle_t conn) {
    if (!conn) {
        return;
    }

    if (conn->state) {
        if (conn->state->txn) {
            ppdb_database_txn_abort(conn->state->txn);
        }
        if (conn->state->socket >= 0) {
            close(conn->state->socket);
        }
        free(conn->state);
    }

    free(conn);
}

// Set socket
ppdb_error_t ppdb_conn_set_socket(ppdb_handle_t conn, int socket) {
    if (!conn || !conn->state || socket < 0) {
        return PPDB_ERR_PARAM;
    }

    conn->state->socket = socket;
    conn->state->connected = true;
    return PPDB_OK;
}

// Send data
ppdb_error_t ppdb_conn_send(ppdb_handle_t conn, const void* data, size_t size) {
    if (!conn || !conn->state || !data || !size) {
        return PPDB_ERR_PARAM;
    }

    if (!conn->state->connected || conn->state->socket < 0) {
        return PPDB_ERR_NOT_CONNECTED;
    }

    ssize_t sent = send(conn->state->socket, data, size, 0);
    if (sent < 0) {
        return PPDB_ERR_IO;
    }

    return PPDB_OK;
}

// Receive data
ppdb_error_t ppdb_conn_recv(ppdb_handle_t conn, void* data, size_t size) {
    if (!conn || !conn->state || !data || !size) {
        return PPDB_ERR_PARAM;
    }

    if (!conn->state->connected || conn->state->socket < 0) {
        return PPDB_ERR_NOT_CONNECTED;
    }

    ssize_t received = recv(conn->state->socket, data, size, 0);
    if (received < 0) {
        return PPDB_ERR_IO;
    }

    return PPDB_OK;
}

// Close connection
void ppdb_conn_close(ppdb_handle_t conn) {
    if (!conn || !conn->state) {
        return;
    }

    if (conn->state->socket >= 0) {
        close(conn->state->socket);
        conn->state->socket = -1;
    }

    conn->state->connected = false;
}