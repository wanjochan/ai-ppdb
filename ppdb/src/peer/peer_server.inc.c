//-----------------------------------------------------------------------------
// Server Context Implementation
//-----------------------------------------------------------------------------

struct ppdb_server_s {
    ppdb_ctx_t ctx;              // Database context
    ppdb_net_config_t config;    // Network configuration
    ppdb_handle_t peer;          // Network peer handle
    bool running;                // Server running flag
};

//-----------------------------------------------------------------------------
// Server Management Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_server_create(ppdb_server_t** server, ppdb_ctx_t ctx, const ppdb_net_config_t* config) {
    if (!server || !ctx || !config) {
        return PPDB_ERR_PARAM;
    }

    // Allocate server context
    ppdb_server_t* srv = (ppdb_server_t*)malloc(sizeof(ppdb_server_t));
    if (!srv) {
        return PPDB_ERR_MEMORY;
    }
    memset(srv, 0, sizeof(ppdb_server_t));

    // Initialize server context
    srv->ctx = ctx;
    srv->config = *config;
    srv->running = false;

    *server = srv;
    return PPDB_OK;
}

ppdb_error_t ppdb_server_start(ppdb_server_t* server) {
    if (!server) {
        return PPDB_ERR_PARAM;
    }

    if (server->running) {
        return PPDB_BASE_ERR_INVALID_STATE;
    }

    // Create peer instance
    ppdb_error_t err = ppdb_conn_create(&server->peer, peer_get_memcached(), server->ctx);
    if (err != PPDB_OK) {
        return err;
    }

    // Start peer
    err = ppdb_conn_set_socket(server->peer, -1);  // Let peer create its own socket
    if (err != PPDB_OK) {
        ppdb_conn_destroy(server->peer);
        server->peer = 0;
        return err;
    }

    server->running = true;
    return PPDB_OK;
}

ppdb_error_t ppdb_server_stop(ppdb_server_t* server) {
    if (!server) {
        return PPDB_ERR_PARAM;
    }

    if (!server->running) {
        return PPDB_BASE_ERR_INVALID_STATE;
    }

    // Stop peer
    ppdb_conn_close(server->peer);

    // Cleanup peer
    ppdb_conn_destroy(server->peer);
    server->peer = 0;
    server->running = false;

    return PPDB_OK;
}

ppdb_error_t ppdb_server_destroy(ppdb_server_t* server) {
    if (!server) {
        return PPDB_ERR_PARAM;
    }

    // Stop server if running
    if (server->running) {
        ppdb_server_stop(server);
    }

    // Free server context
    free(server);
    return PPDB_OK;
} 