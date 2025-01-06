#include "peer_internal.h"

//-----------------------------------------------------------------------------
// Static Functions
//-----------------------------------------------------------------------------

static void on_engine_complete(ppdb_error_t error, void* result, void* user_data) {
    ppdb_peer_connection_t* conn = user_data;
    if (!conn) return;

    // Prepare response
    ppdb_peer_response_t resp = {0};
    resp.error = error;
    resp.flags = conn->current_req.flags;
    resp.cas = conn->current_req.cas;

    if (error == PPDB_OK && result) {
        ppdb_data_t* data = result;
        if (data->size <= sizeof(resp.value.inline_data)) {
            memcpy(resp.value.inline_data, data->inline_data, data->size);
        } else {
            resp.value.extended_data = ppdb_base_alloc(data->size);
            if (!resp.value.extended_data) {
                resp.error = PPDB_ERR_MEMORY;
            } else {
                if (data->extended_data) {
                    memcpy(resp.value.extended_data, data->extended_data, data->size);
                } else {
                    memcpy(resp.value.extended_data, data->inline_data, data->size);
                }
            }
        }
        resp.value.size = data->size;
        ppdb_base_free(result);
    }

    // Send response
    ppdb_peer_async_complete(conn, resp.error, &resp);
}

static ppdb_error_t handle_get(ppdb_peer_connection_t* conn,
                             const ppdb_peer_request_t* req) {
    ppdb_peer_t* peer = conn->peer;
    if (!peer || !peer->engine) {
        return PPDB_ERR_INTERNAL;
    }

    // Start async get operation
    return ppdb_engine_async_get(peer->engine,
                               &req->key,
                               on_engine_complete,
                               conn);
}

static ppdb_error_t handle_set(ppdb_peer_connection_t* conn,
                             const ppdb_peer_request_t* req) {
    ppdb_peer_t* peer = conn->peer;
    if (!peer || !peer->engine) {
        return PPDB_ERR_INTERNAL;
    }

    // Start async set operation
    return ppdb_engine_async_put(peer->engine,
                               &req->key,
                               &req->value,
                               on_engine_complete,
                               conn);
}

static ppdb_error_t handle_delete(ppdb_peer_connection_t* conn,
                                const ppdb_peer_request_t* req) {
    ppdb_peer_t* peer = conn->peer;
    if (!peer || !peer->engine) {
        return PPDB_ERR_INTERNAL;
    }

    // Start async delete operation
    return ppdb_engine_async_delete(peer->engine,
                                  &req->key,
                                  on_engine_complete,
                                  conn);
}

static ppdb_error_t handle_stats(ppdb_peer_connection_t* conn,
                               const ppdb_peer_request_t* req) {
    ppdb_peer_t* peer = conn->peer;
    if (!peer) {
        return PPDB_ERR_INTERNAL;
    }

    // Prepare stats buffer
    char* stats = ppdb_base_alloc(1024);
    if (!stats) {
        return PPDB_ERR_MEMORY;
    }

    // Get stats
    ppdb_error_t err = ppdb_peer_get_stats(peer, stats, 1024);
    if (err != PPDB_OK) {
        ppdb_base_free(stats);
        return err;
    }

    // Prepare response
    ppdb_peer_response_t resp = {0};
    resp.error = PPDB_OK;
    resp.flags = req->flags;
    resp.cas = req->cas;

    size_t stats_len = strlen(stats);
    if (stats_len <= sizeof(resp.value.inline_data)) {
        memcpy(resp.value.inline_data, stats, stats_len);
    } else {
        resp.value.extended_data = stats;
        stats = NULL;  // Transfer ownership
    }
    resp.value.size = stats_len;

    // Send response
    ppdb_peer_async_complete(conn, PPDB_OK, &resp);

    if (stats) {
        ppdb_base_free(stats);
    }
    return PPDB_OK;
}

//-----------------------------------------------------------------------------
// Public Functions
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_peer_async_handle_request(ppdb_peer_connection_t* conn,
                                          const ppdb_peer_request_t* req) {
    if (!conn || !req) {
        return PPDB_ERR_PARAM;
    }

    ppdb_peer_t* peer = conn->peer;
    if (!peer) {
        return PPDB_ERR_INTERNAL;
    }

    // Update request stats
    ppdb_base_mutex_lock(peer->mutex);
    peer->stats.total_requests++;
    ppdb_base_mutex_unlock(peer->mutex);

    // Handle request based on type
    ppdb_error_t err;
    switch (req->type) {
        case PPDB_PEER_REQ_GET:
            err = handle_get(conn, req);
            break;
        case PPDB_PEER_REQ_SET:
            err = handle_set(conn, req);
            break;
        case PPDB_PEER_REQ_DELETE:
            err = handle_delete(conn, req);
            break;
        case PPDB_PEER_REQ_STATS:
            err = handle_stats(conn, req);
            break;
        default:
            err = PPDB_ERR_PROTOCOL;
            break;
    }

    if (err != PPDB_OK) {
        ppdb_base_mutex_lock(peer->mutex);
        peer->stats.failed_requests++;
        ppdb_base_mutex_unlock(peer->mutex);
    }

    return err;
}

void ppdb_peer_async_complete(ppdb_peer_connection_t* conn,
                            ppdb_error_t error,
                            const ppdb_peer_response_t* resp) {
    if (!conn) return;

    // Call user callback if set
    if (conn->callback) {
        conn->callback(conn, resp, conn->user_data);
    }

    // Format and send response
    if (resp) {
        size_t len = conn->write.size;
        ppdb_error_t err = ppdb_peer_proto_format(conn, resp, conn->write.buf, &len);
        if (err == PPDB_OK) {
            conn->write.size = len;
            ppdb_peer_conn_start_write(conn);
        }
    }

    // Reset state
    conn->proto_state = PPDB_PEER_PROTO_INIT;
    conn->callback = NULL;
    conn->user_data = NULL;
    cleanup_request(&conn->current_req);
    cleanup_response(&conn->current_resp);
}