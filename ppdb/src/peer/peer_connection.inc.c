#ifndef PPDB_PEER_CONNECTION_INC_C_
#define PPDB_PEER_CONNECTION_INC_C_

#include <cosmopolitan.h>
#include "../internal/peer.h"
#include "../internal/database.h"
#include "internal/base.h"

// Connection state
typedef struct ppdb_connection_state {
    ppdb_ctx_t* ctx;                // Database context
    ppdb_database_txn_t* txn;       // Current transaction
    ppdb_database_table_t* table;   // Current table
    int socket;                     // Socket descriptor
    void* proto_data;               // Protocol-specific data
    bool connected;                 // Connection status
    ppdb_base_mutex_t mutex;        // Connection mutex
} ppdb_connection_state_t;

// Create connection
ppdb_error_t ppdb_connection_create(ppdb_ctx_t* ctx, ppdb_connection_state_t** conn) {
    if (!ctx || !conn) {
        return PPDB_ERR_PARAM;
    }

    // Allocate connection state
    ppdb_connection_state_t* state = calloc(1, sizeof(ppdb_connection_state_t));
    if (!state) {
        return PPDB_ERR_MEMORY;
    }

    // Initialize state
    state->ctx = ctx;
    state->connected = false;
    state->socket = -1;

    // Initialize mutex
    if (ppdb_base_mutex_create(&state->mutex) != 0) {
        free(state);
        return PPDB_ERR_MUTEX;
    }

    // Create transaction
    ppdb_error_t err = ppdb_database_txn_begin(ctx->db, NULL, 0, &state->txn);
    if (err != PPDB_OK) {
        ppdb_base_mutex_destroy(&state->mutex);
        free(state);
        return err;
    }

    *conn = state;
    return PPDB_OK;
}

// Destroy connection
void ppdb_connection_destroy(ppdb_connection_state_t* conn) {
    if (!conn) {
        return;
    }

    if (conn->txn) {
        ppdb_database_txn_abort(conn->txn);
    }

    if (conn->socket >= 0) {
        close(conn->socket);
    }

    ppdb_base_mutex_destroy(&conn->mutex);
    free(conn);
}

// Set socket
ppdb_error_t ppdb_connection_set_socket(ppdb_connection_state_t* conn, int socket) {
    if (!conn || socket < 0) {
        return PPDB_ERR_PARAM;
    }

    ppdb_base_mutex_lock(&conn->mutex);
    conn->socket = socket;
    conn->connected = true;
    ppdb_base_mutex_unlock(&conn->mutex);

    return PPDB_OK;
}

// Send data
ppdb_error_t ppdb_connection_send(ppdb_connection_state_t* conn, const void* data, size_t size) {
    if (!conn || !data || !size) {
        return PPDB_ERR_PARAM;
    }

    ppdb_base_mutex_lock(&conn->mutex);

    if (!conn->connected || conn->socket < 0) {
        ppdb_base_mutex_unlock(&conn->mutex);
        return PPDB_ERR_NOT_CONNECTED;
    }

    ssize_t sent = send(conn->socket, data, size, 0);
    ppdb_base_mutex_unlock(&conn->mutex);

    if (sent < 0) {
        return PPDB_ERR_IO;
    }

    return PPDB_OK;
}

// Receive data
ppdb_error_t ppdb_connection_recv(ppdb_connection_state_t* conn, void* data, size_t size) {
    if (!conn || !data || !size) {
        return PPDB_ERR_PARAM;
    }

    ppdb_base_mutex_lock(&conn->mutex);

    if (!conn->connected || conn->socket < 0) {
        ppdb_base_mutex_unlock(&conn->mutex);
        return PPDB_ERR_NOT_CONNECTED;
    }

    ssize_t received = recv(conn->socket, data, size, 0);
    ppdb_base_mutex_unlock(&conn->mutex);

    if (received < 0) {
        return PPDB_ERR_IO;
    }

    return PPDB_OK;
}

// Close connection
void ppdb_connection_close(ppdb_connection_state_t* conn) {
    if (!conn) {
        return;
    }

    ppdb_base_mutex_lock(&conn->mutex);

    if (conn->socket >= 0) {
        close(conn->socket);
        conn->socket = -1;
    }

    conn->connected = false;
    ppdb_base_mutex_unlock(&conn->mutex);
}

#endif // PPDB_PEER_CONNECTION_INC_C_
