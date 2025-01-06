#include <cosmopolitan.h>

#include "client.h"
#include "../common/protocol.h"
#include "peer_internal.h"

struct ppdb_client {
    int socket_fd;
    char *server_addr;
    int server_port;
    int connected;
};

ppdb_client_t* ppdb_client_create(const char *addr, int port) {
    ppdb_client_t *client = malloc(sizeof(ppdb_client_t));
    if (!client) return NULL;
    
    client->server_addr = strdup(addr);
    client->server_port = port;
    client->connected = 0;
    client->socket_fd = -1;
    
    return client;
}

int ppdb_client_connect(ppdb_client_t *client) {
    if (!client) return -1;
    
    client->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->socket_fd < 0) return -1;
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(client->server_port);
    inet_pton(AF_INET, client->server_addr, &server_addr.sin_addr);
    
    if (connect(client->socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(client->socket_fd);
        return -1;
    }
    
    client->connected = 1;
    return 0;
}

void ppdb_client_close(ppdb_client_t *client) {
    if (!client) return;
    
    if (client->socket_fd >= 0) {
        close(client->socket_fd);
    }
    
    free(client->server_addr);
    free(client);
}

// Basic operation interfaces
int ppdb_client_put(ppdb_client_t *client, const char *key, const char *value) {
    // TODO: Implement put operation
    return 0;
}

char* ppdb_client_get(ppdb_client_t *client, const char *key) {
    // TODO: Implement get operation
    return NULL;
}

int ppdb_client_delete(ppdb_client_t *client, const char *key) {
    // TODO: Implement delete operation
    return 0;
}

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

static ppdb_client_ctx_t* client_ctx_create(ppdb_ctx_t db_ctx) {
    ppdb_client_ctx_t* ctx = ppdb_base_alloc(sizeof(ppdb_client_ctx_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(ppdb_client_ctx_t));
    ctx->db_ctx = db_ctx;
    return ctx;
}

static void client_ctx_destroy(ppdb_client_ctx_t* ctx) {
    if (!ctx) return;

    if (ctx->conn) {
        ppdb_peer_disconnect(ctx->conn);
    }
    if (ctx->peer) {
        ppdb_peer_stop(ctx->peer);
        ppdb_peer_destroy(ctx->peer);
    }
    ppdb_base_free(ctx);
}

//-----------------------------------------------------------------------------
// Static Functions
//-----------------------------------------------------------------------------

static void on_peer_connection(ppdb_peer_connection_t* conn,
                             ppdb_error_t error,
                             void* user_data) {
    ppdb_client_ctx_t* ctx = user_data;
    if (!ctx || !ctx->cb) return;

    if (error == PPDB_OK) {
        ctx->conn = conn;
        ctx->connected = true;
    }

    // Call user callback
    ctx->cb((ppdb_conn_t)conn, error, ctx->user_data);
}

static void on_operation_complete(ppdb_peer_connection_t* conn,
                                const ppdb_peer_response_t* resp,
                                void* user_data) {
    ppdb_complete_callback cb = user_data;
    if (!cb) return;

    // Call user callback
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
    ppdb_client_ctx_t* client_ctx = client_ctx_create(ctx);
    if (!client_ctx) {
        return PPDB_ERR_MEMORY;
    }

    // Create peer configuration
    ppdb_peer_config_t peer_config = {
        .host = config->host,
        .port = config->port,
        .timeout_ms = config->timeout_ms,
        .max_connections = 1,  // Client only needs one connection
        .io_threads = 1,      // Client only needs one thread
        .use_tcp_nodelay = config->use_tcp_nodelay,
        .is_server = false    // Client mode
    };

    // Create peer instance
    ppdb_error_t err = ppdb_peer_create(&client_ctx->peer, &peer_config, NULL);
    if (err != PPDB_OK) {
        client_ctx_destroy(client_ctx);
        return err;
    }

    // Set connection callback
    err = ppdb_peer_set_connection_callback(client_ctx->peer,
                                          on_peer_connection,
                                          client_ctx);
    if (err != PPDB_OK) {
        client_ctx_destroy(client_ctx);
        return err;
    }

    // Start peer
    err = ppdb_peer_start(client_ctx->peer);
    if (err != PPDB_OK) {
        client_ctx_destroy(client_ctx);
        return err;
    }

    // Connect to server
    err = ppdb_peer_connect(client_ctx->peer,
                           config->host,
                           config->port,
                           &client_ctx->conn);
    if (err != PPDB_OK) {
        client_ctx_destroy(client_ctx);
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

    client_ctx_destroy(ctx);
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