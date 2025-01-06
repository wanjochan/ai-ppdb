#include "peer_internal.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define PPDB_CONN_READ_BUF_SIZE  8192
#define PPDB_CONN_WRITE_BUF_SIZE 8192

//-----------------------------------------------------------------------------
// Static Functions
//-----------------------------------------------------------------------------

static void cleanup_request(ppdb_peer_request_t* req) {
    if (!req) return;

    if (req->key.extended_data) {
        ppdb_base_free(req->key.extended_data);
        req->key.extended_data = NULL;
    }
    if (req->value.extended_data) {
        ppdb_base_free(req->value.extended_data);
        req->value.extended_data = NULL;
    }
}

static void cleanup_response(ppdb_peer_response_t* resp) {
    if (!resp) return;

    if (resp->value.extended_data) {
        ppdb_base_free(resp->value.extended_data);
        resp->value.extended_data = NULL;
    }
}

static void on_read(ppdb_base_async_handle_t* handle, int status) {
    ppdb_peer_connection_t* conn = handle->user_data;
    if (!conn) return;

    if (status != 0) {
        // Read error
        ppdb_peer_async_complete(conn, PPDB_ERR_IO, NULL);
        return;
    }

    // Parse protocol data
    ppdb_error_t err = ppdb_peer_proto_parse(conn, 
                                           conn->read.buf + conn->read.pos,
                                           conn->read.size - conn->read.pos);
    if (err != PPDB_OK) {
        ppdb_peer_async_complete(conn, err, NULL);
        return;
    }

    // Continue reading if needed
    if (conn->proto_state != PPDB_PEER_PROTO_COMPLETE) {
        ppdb_peer_conn_start_read(conn);
        return;
    }

    // Handle complete request
    err = ppdb_peer_async_handle_request(conn, &conn->current_req);
    if (err != PPDB_OK) {
        ppdb_peer_async_complete(conn, err, NULL);
        return;
    }
}

static void on_write(ppdb_base_async_handle_t* handle, int status) {
    ppdb_peer_connection_t* conn = handle->user_data;
    if (!conn) return;

    if (status != 0) {
        // Write error
        ppdb_peer_async_complete(conn, PPDB_ERR_IO, NULL);
        return;
    }

    // Reset write buffer
    conn->write.pos = 0;

    // Start reading next request
    ppdb_peer_conn_start_read(conn);
}

//-----------------------------------------------------------------------------
// Public Functions
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_peer_conn_create(ppdb_peer_t* peer,
                                  ppdb_peer_connection_t** conn) {
    if (!peer || !conn) {
        return PPDB_ERR_PARAM;
    }

    *conn = ppdb_base_alloc(sizeof(ppdb_peer_connection_t));
    if (!*conn) {
        return PPDB_ERR_MEMORY;
    }

    memset(*conn, 0, sizeof(ppdb_peer_connection_t));
    (*conn)->peer = peer;
    (*conn)->state = PPDB_PEER_CONN_INIT;
    (*conn)->proto_state = PPDB_PEER_PROTO_INIT;

    // Allocate read buffer
    (*conn)->read.buf = ppdb_base_alloc(PPDB_CONN_READ_BUF_SIZE);
    if (!(*conn)->read.buf) {
        ppdb_peer_conn_destroy(*conn);
        *conn = NULL;
        return PPDB_ERR_MEMORY;
    }
    (*conn)->read.size = PPDB_CONN_READ_BUF_SIZE;

    // Allocate write buffer
    (*conn)->write.buf = ppdb_base_alloc(PPDB_CONN_WRITE_BUF_SIZE);
    if (!(*conn)->write.buf) {
        ppdb_peer_conn_destroy(*conn);
        *conn = NULL;
        return PPDB_ERR_MEMORY;
    }
    (*conn)->write.size = PPDB_CONN_WRITE_BUF_SIZE;

    // Update peer stats
    ppdb_base_mutex_lock(peer->mutex);
    peer->stats.total_connections++;
    peer->stats.active_connections++;
    ppdb_base_mutex_unlock(peer->mutex);

    return PPDB_OK;
}

void ppdb_peer_conn_destroy(ppdb_peer_connection_t* conn) {
    if (!conn) return;

    // Update peer stats
    if (conn->peer) {
        ppdb_base_mutex_lock(conn->peer->mutex);
        conn->peer->stats.active_connections--;
        ppdb_base_mutex_unlock(conn->peer->mutex);
    }

    // Cleanup request/response data
    cleanup_request(&conn->current_req);
    cleanup_response(&conn->current_resp);

    // Free buffers
    if (conn->read.buf) {
        ppdb_base_free(conn->read.buf);
    }
    if (conn->write.buf) {
        ppdb_base_free(conn->write.buf);
    }

    // Destroy async handle
    if (conn->handle) {
        ppdb_base_async_handle_destroy(conn->handle);
    }

    ppdb_base_free(conn);
}

ppdb_error_t ppdb_peer_conn_start_read(ppdb_peer_connection_t* conn) {
    if (!conn || !conn->handle) {
        return PPDB_ERR_PARAM;
    }

    // Reset read buffer if needed
    if (conn->read.pos >= conn->read.size) {
        conn->read.pos = 0;
    }

    // Start async read
    return ppdb_base_async_read(conn->handle,
                              conn->read.buf + conn->read.pos,
                              conn->read.size - conn->read.pos,
                              on_read);
}

ppdb_error_t ppdb_peer_conn_start_write(ppdb_peer_connection_t* conn) {
    if (!conn || !conn->handle) {
        return PPDB_ERR_PARAM;
    }

    // Start async write
    return ppdb_base_async_write(conn->handle,
                               conn->write.buf + conn->write.pos,
                               conn->write.size - conn->write.pos,
                               on_write);
}