#include "peer_internal.h"

//-----------------------------------------------------------------------------
// Context Management
//-----------------------------------------------------------------------------

typedef struct ppdb_client_ctx_s {
    ppdb_ctx_t db_ctx;         // Database context
    ppdb_peer_t* peer;         // Peer instance
    ppdb_peer_connection_t* conn; // Active connection
    ppdb_conn_callback cb;     // User connection callback
    void* user_data;          // User callback data
    bool connected;           // Connection state
} ppdb_client_ctx_t;

static void on_peer_connection(ppdb_peer_connection_t* conn,
                             ppdb_error_t error,
                             void* user_data) {
    ppdb_client_ctx_t* ctx = user_data;
    if (!ctx || !ctx->cb) return;

    if (error == PPDB_OK) {
        ctx->conn = conn;
        ctx->connected = true;
    }

    // Forward to user callback
    ctx->cb((ppdb_conn_t)conn, error, ctx->user_data);
}

static void on_operation_complete(ppdb_peer_connection_t* conn,
                                const ppdb_peer_response_t* resp,
                                void* user_data) {
    ppdb_complete_callback cb = user_data;
    if (!cb) return;

    // Forward to user callback
    cb(resp->error, (void*)&resp->value, NULL);
}

//-----------------------------------------------------------------------------
// Public Functions
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_client_connect(ppdb_ctx_t ctx,
                                const ppdb_net_config_t* config,
                                ppdb_conn_t* conn) {
    if (!ctx || !config || !conn) {
        return PPDB_ERR_PARAM;
    }

    // Create client context
    ppdb_client_ctx_t* client_ctx = ppdb_base_alloc(sizeof(ppdb_client_ctx_t));
    if (!client_ctx) {
        return PPDB_ERR_MEMORY;
    }

    memset(client_ctx, 0, sizeof(ppdb_client_ctx_t));
    client_ctx->db_ctx = ctx;

    // Create peer configuration
    ppdb_peer_config_t peer_config = {
        .host = config->host,
        .port = config->port,
        .timeout_ms = config->timeout_ms,
        .max_connections = 1,  // Client only needs one connection
        .io_threads = 1,      // Client only needs one thread
        .use_tcp_nodelay = config->use_tcp_nodelay,
        .mode = PPDB_PEER_MODE_CLIENT
    };

    // Create peer instance
    ppdb_error_t err = ppdb_peer_create(&client_ctx->peer, &peer_config, NULL);
    if (err != PPDB_OK) {
        ppdb_base_free(client_ctx);
        return err;
    }

    // Set connection callback
    err = ppdb_peer_set_connection_callback(client_ctx->peer,
                                          on_peer_connection,
                                          client_ctx);
    if (err != PPDB_OK) {
        ppdb_peer_destroy(client_ctx->peer);
        ppdb_base_free(client_ctx);
        return err;
    }

    // Start peer
    err = ppdb_peer_start(client_ctx->peer);
    if (err != PPDB_OK) {
        ppdb_peer_destroy(client_ctx->peer);
        ppdb_base_free(client_ctx);
        return err;
    }

    // Connect to server
    err = ppdb_peer_connect(client_ctx->peer,
                           config->host,
                           config->port,
                           &client_ctx->conn);
    if (err != PPDB_OK) {
        ppdb_peer_destroy(client_ctx->peer);
        ppdb_base_free(client_ctx);
        return err;
    }

    *conn = (ppdb_conn_t)client_ctx;
    return PPDB_OK;
}

ppdb_error_t ppdb_client_disconnect(ppdb_conn_t conn) {
    if (!conn) {
        return PPDB_ERR_PARAM;
    }

    ppdb_client_ctx_t* ctx = (ppdb_client_ctx_t*)conn;
    if (!ctx->connected) {
        return PPDB_OK;
    }

    if (ctx->conn) {
        ppdb_peer_disconnect(ctx->conn);
    }

    ppdb_peer_destroy(ctx->peer);
    ppdb_base_free(ctx);
    return PPDB_OK;
}

ppdb_error_t ppdb_client_get(ppdb_conn_t conn,
                            const ppdb_data_t* key,
                            ppdb_complete_callback cb,
                            void* user_data) {
    if (!conn || !key || !cb) {
        return PPDB_ERR_PARAM;
    }

    ppdb_client_ctx_t* ctx = (ppdb_client_ctx_t*)conn;
    if (!ctx->connected || !ctx->conn) {
        return PPDB_ERR_NOT_CONNECTED;
    }

    // Prepare request
    ppdb_peer_request_t req = {
        .type = PPDB_PEER_REQ_GET,
        .key = *key
    };

    // Send request
    return ppdb_peer_async_request(ctx->conn,
                                 &req,
                                 on_operation_complete,
                                 cb);
}

ppdb_error_t ppdb_client_put(ppdb_conn_t conn,
                            const ppdb_data_t* key,
                            const ppdb_data_t* value,
                            ppdb_complete_callback cb,
                            void* user_data) {
    if (!conn || !key || !value || !cb) {
        return PPDB_ERR_PARAM;
    }

    ppdb_client_ctx_t* ctx = (ppdb_client_ctx_t*)conn;
    if (!ctx->connected || !ctx->conn) {
        return PPDB_ERR_NOT_CONNECTED;
    }

    // Prepare request
    ppdb_peer_request_t req = {
        .type = PPDB_PEER_REQ_SET,
        .key = *key,
        .value = *value
    };

    // Send request
    return ppdb_peer_async_request(ctx->conn,
                                 &req,
                                 on_operation_complete,
                                 cb);
}

ppdb_error_t ppdb_client_delete(ppdb_conn_t conn,
                               const ppdb_data_t* key,
                               ppdb_complete_callback cb,
                               void* user_data) {
    if (!conn || !key || !cb) {
        return PPDB_ERR_PARAM;
    }

    ppdb_client_ctx_t* ctx = (ppdb_client_ctx_t*)conn;
    if (!ctx->connected || !ctx->conn) {
        return PPDB_ERR_NOT_CONNECTED;
    }

    // Prepare request
    ppdb_peer_request_t req = {
        .type = PPDB_PEER_REQ_DELETE,
        .key = *key
    };

    // Send request
    return ppdb_peer_async_request(ctx->conn,
                                 &req,
                                 on_operation_complete,
                                 cb);
}