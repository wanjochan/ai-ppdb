#include "peer_internal.h"

//-----------------------------------------------------------------------------
// Context Management
//-----------------------------------------------------------------------------

typedef struct ppdb_server_ctx_s {
    ppdb_ctx_t db_ctx;         // Database context
    ppdb_peer_t* peer;         // Peer instance
    ppdb_conn_callback cb;     // User connection callback
    void* user_data;          // User callback data
} ppdb_server_ctx_t;

static void on_peer_connection(ppdb_peer_connection_t* conn,
                             ppdb_error_t error,
                             void* user_data) {
    ppdb_server_ctx_t* ctx = user_data;
    if (!ctx || !ctx->cb) return;

    // Forward to user callback
    ctx->cb((ppdb_conn_t)conn, error, ctx->user_data);
}

//-----------------------------------------------------------------------------
// Public Functions
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_server_start(ppdb_ctx_t ctx, const ppdb_net_config_t* config) {
    if (!ctx || !config) {
        return PPDB_ERR_PARAM;
    }

    // Create server context
    ppdb_server_ctx_t* server_ctx = ppdb_base_alloc(sizeof(ppdb_server_ctx_t));
    if (!server_ctx) {
        return PPDB_ERR_MEMORY;
    }

    memset(server_ctx, 0, sizeof(ppdb_server_ctx_t));
    server_ctx->db_ctx = ctx;

    // Get storage engine
    ppdb_engine_t* engine;
    ppdb_error_t err = ppdb_engine_get(ctx, &engine);
    if (err != PPDB_OK) {
        ppdb_base_free(server_ctx);
        return err;
    }

    // Create peer configuration
    ppdb_peer_config_t peer_config = {
        .host = config->host,
        .port = config->port,
        .timeout_ms = config->timeout_ms,
        .max_connections = config->max_connections,
        .io_threads = config->io_threads,
        .use_tcp_nodelay = config->use_tcp_nodelay,
        .mode = PPDB_PEER_MODE_SERVER
    };

    // Create peer instance
    err = ppdb_peer_create(&server_ctx->peer, &peer_config, engine);
    if (err != PPDB_OK) {
        ppdb_base_free(server_ctx);
        return err;
    }

    // Set connection callback
    err = ppdb_peer_set_connection_callback(server_ctx->peer,
                                          on_peer_connection,
                                          server_ctx);
    if (err != PPDB_OK) {
        ppdb_peer_destroy(server_ctx->peer);
        ppdb_base_free(server_ctx);
        return err;
    }

    // Start peer
    err = ppdb_peer_start(server_ctx->peer);
    if (err != PPDB_OK) {
        ppdb_peer_destroy(server_ctx->peer);
        ppdb_base_free(server_ctx);
        return err;
    }

    return PPDB_OK;
}

ppdb_error_t ppdb_server_stop(ppdb_ctx_t ctx) {
    if (!ctx) {
        return PPDB_ERR_PARAM;
    }

    ppdb_server_ctx_t* server_ctx = (ppdb_server_ctx_t*)ctx;
    if (!server_ctx->peer) {
        return PPDB_OK;
    }

    // Stop peer
    ppdb_error_t err = ppdb_peer_stop(server_ctx->peer);
    if (err != PPDB_OK) {
        return err;
    }

    ppdb_peer_destroy(server_ctx->peer);
    ppdb_base_free(server_ctx);
    return PPDB_OK;
}

ppdb_error_t ppdb_server_set_conn_callback(ppdb_ctx_t ctx,
                                         ppdb_conn_callback cb,
                                         void* user_data) {
    if (!ctx) {
        return PPDB_ERR_PARAM;
    }

    ppdb_server_ctx_t* server_ctx = (ppdb_server_ctx_t*)ctx;
    server_ctx->cb = cb;
    server_ctx->user_data = user_data;

    return PPDB_OK;
}

ppdb_error_t ppdb_server_get_stats(ppdb_ctx_t ctx,
                                  char* buffer,
                                  size_t size) {
    if (!ctx || !buffer || size == 0) {
        return PPDB_ERR_PARAM;
    }

    ppdb_server_ctx_t* server_ctx = (ppdb_server_ctx_t*)ctx;
    if (!server_ctx->peer) {
        return PPDB_ERR_INTERNAL;
    }

    return ppdb_peer_get_stats(server_ctx->peer, buffer, size);
}