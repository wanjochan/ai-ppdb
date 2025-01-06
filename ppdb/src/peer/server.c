#include "peer_internal.h"

//-----------------------------------------------------------------------------
// Context Management
//-----------------------------------------------------------------------------

typedef struct ppdb_server_ctx_s {
    ppdb_ctx_t db_ctx;         // Database context
    ppdb_engine_t* engine;     // Storage engine
    ppdb_peer_t* peer;         // Peer instance
    ppdb_conn_callback cb;     // User connection callback
    void* user_data;          // User callback data
    bool running;             // Server running flag
} ppdb_server_ctx_t;

static ppdb_server_ctx_t* server_ctx_create(ppdb_ctx_t db_ctx) {
    ppdb_server_ctx_t* ctx = ppdb_base_alloc(sizeof(ppdb_server_ctx_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(ppdb_server_ctx_t));
    ctx->db_ctx = db_ctx;
    return ctx;
}

static void server_ctx_destroy(ppdb_server_ctx_t* ctx) {
    if (!ctx) return;

    if (ctx->peer) {
        ppdb_peer_stop(ctx->peer);
        ppdb_peer_destroy(ctx->peer);
    }
    ppdb_base_free(ctx);
}

//-----------------------------------------------------------------------------
// Static Functions
//-----------------------------------------------------------------------------

static void on_client_connection(ppdb_peer_connection_t* conn,
                               ppdb_error_t error,
                               void* user_data) {
    ppdb_server_ctx_t* ctx = user_data;
    if (!ctx || !ctx->cb) return;

    // Call user callback
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
    ppdb_server_ctx_t* server_ctx = server_ctx_create(ctx);
    if (!server_ctx) {
        return PPDB_ERR_MEMORY;
    }

    // Get storage engine
    ppdb_error_t err = ppdb_engine_get(ctx, &server_ctx->engine);
    if (err != PPDB_OK) {
        server_ctx_destroy(server_ctx);
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
        .is_server = true  // Server mode
    };

    // Create peer instance
    err = ppdb_peer_create(&server_ctx->peer, &peer_config, server_ctx->engine);
    if (err != PPDB_OK) {
        server_ctx_destroy(server_ctx);
        return err;
    }

    // Set connection callback
    err = ppdb_peer_set_connection_callback(server_ctx->peer, 
                                          on_client_connection, 
                                          server_ctx);
    if (err != PPDB_OK) {
        server_ctx_destroy(server_ctx);
        return err;
    }

    // Start peer
    err = ppdb_peer_start(server_ctx->peer);
    if (err != PPDB_OK) {
        server_ctx_destroy(server_ctx);
        return err;
    }

    server_ctx->running = true;
    return PPDB_OK;
}

ppdb_error_t ppdb_server_stop(ppdb_ctx_t ctx) {
    if (!ctx) {
        return PPDB_ERR_PARAM;
    }

    ppdb_server_ctx_t* server_ctx = (ppdb_server_ctx_t*)ctx;
    if (!server_ctx->running) {
        return PPDB_OK;
    }

    // Stop peer
    ppdb_error_t err = ppdb_peer_stop(server_ctx->peer);
    if (err != PPDB_OK) {
        return err;
    }

    server_ctx->running = false;
    server_ctx_destroy(server_ctx);
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