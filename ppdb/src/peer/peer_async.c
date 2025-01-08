#include "peer_internal.h"
#include "../internal/database.h"
#include "internal/base.h"

//-----------------------------------------------------------------------------
// Static Functions
//-----------------------------------------------------------------------------

static void on_database_complete(ppdb_error_t error, void* result, void* user_data) {
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
            resp.value.extended_data = malloc(data->size);
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
        free(result);
    }

    // Send response
    ppdb_peer_async_complete(conn, resp.error, &resp);
}

static ppdb_error_t handle_get(ppdb_peer_connection_t* conn,
                             const ppdb_peer_request_t* req) {
    ppdb_peer_t* peer = conn->peer;
    if (!peer || !peer->db) {
        return PPDB_ERR_INTERNAL;
    }

    // Start async get operation
    return ppdb_database_get(peer->db,
                           conn->txn,
                           "default",
                           req->key.data,
                           req->key.size,
                           &req->value.data,
                           &req->value.size);
}

static ppdb_error_t handle_set(ppdb_peer_connection_t* conn,
                             const ppdb_peer_request_t* req) {
    ppdb_peer_t* peer = conn->peer;
    if (!peer || !peer->db) {
        return PPDB_ERR_INTERNAL;
    }

    // Start async set operation
    return ppdb_database_put(peer->db,
                           conn->txn,
                           "default",
                           req->key.data,
                           req->key.size,
                           req->value.data,
                           req->value.size);
}

static ppdb_error_t handle_delete(ppdb_peer_connection_t* conn,
                                const ppdb_peer_request_t* req) {
    ppdb_peer_t* peer = conn->peer;
    if (!peer || !peer->db) {
        return PPDB_ERR_INTERNAL;
    }

    // Start async delete operation
    return ppdb_database_delete(peer->db,
                              conn->txn,
                              "default",
                              req->key.data,
                              req->key.size);
}

static ppdb_error_t handle_stats(ppdb_peer_connection_t* conn,
                               const ppdb_peer_request_t* req) {
    ppdb_peer_t* peer = conn->peer;
    if (!peer || !peer->db) {
        return PPDB_ERR_INTERNAL;
    }

    // Get database stats
    ppdb_database_stats_t stats;
    ppdb_error_t err = ppdb_database_get_stats(peer->db, &stats);
    if (err != PPDB_OK) {
        return err;
    }

    // Format stats string
    char* stats_str = malloc(1024);
    if (!stats_str) {
        return PPDB_ERR_MEMORY;
    }

    int len = snprintf(stats_str, 1024,
        "total_txns: %lu\n"
        "committed_txns: %lu\n"
        "aborted_txns: %lu\n"
        "conflicts: %lu\n"
        "deadlocks: %lu\n"
        "cache_hits: %lu\n"
        "cache_misses: %lu\n"
        "bytes_written: %lu\n"
        "bytes_read: %lu\n",
        stats.total_txns,
        stats.committed_txns,
        stats.aborted_txns,
        stats.conflicts,
        stats.deadlocks,
        stats.cache_hits,
        stats.cache_misses,
        stats.bytes_written,
        stats.bytes_read);

    if (len < 0 || len >= 1024) {
        free(stats_str);
        return PPDB_ERR_BUFFER_FULL;
    }

    // Prepare response
    ppdb_peer_response_t resp = {0};
    resp.error = PPDB_OK;
    resp.value.extended_data = stats_str;
    resp.value.size = len;

    // Send response
    ppdb_peer_async_complete(conn, PPDB_OK, &resp);

    free(stats_str);
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

    ppdb_base_mutex_lock(&conn->mutex);
   conn->current_req = *req;
    ppdb_base_mutex_unlock(&conn->mutex);

    ppdb_error_t err;
    switch (req->type) {
        case PPDB_REQ_GET:
            err = handle_get(conn, req);
            break;

        case PPDB_REQ_SET:
            err = handle_set(conn, req);
            break;

        case PPDB_REQ_DELETE:
            err = handle_delete(conn, req);
            break;

        case PPDB_REQ_STATS:
            err = handle_stats(conn, req);
            break;

        default:
            err = PPDB_ERR_INVALID_REQUEST;
            break;
    }

    return err;
}

void ppdb_peer_async_complete(ppdb_peer_connection_t* conn,
                            ppdb_error_t error,
                            const ppdb_peer_response_t* resp) {
    if (!conn) {
        return;
    }

    ppdb_base_mutex_lock(&conn->mutex);

    // Send response
    if (conn->response_cb) {
        conn->response_cb(conn, error, resp);
    }

    ppdb_base_mutex_unlock(&conn->mutex);
}