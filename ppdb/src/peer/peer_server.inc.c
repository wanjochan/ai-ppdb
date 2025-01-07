#ifndef PPDB_PEER_SERVER_INC_C_
#define PPDB_PEER_SERVER_INC_C_

#include <cosmopolitan.h>
#include "../internal/peer.h"
#include "../internal/base.h"

//-----------------------------------------------------------------------------
// Server Types
//-----------------------------------------------------------------------------

// Client state
typedef struct client_state {
    void* peer_data;                // Protocol-specific data
    struct ppdb_server* server;     // Server instance
    ppdb_base_async_handle_t* handle; // Client handle
} client_state_t;

// Server structure
struct ppdb_server {
    ppdb_ctx_t* ctx;                // Database context
    const peer_ops_t* peer_ops;     // Protocol operations
    ppdb_base_async_loop_t* loop;   // Event loop
    ppdb_base_async_handle_t* listen_handle; // Listen handle
    bool running;                   // Server state
};

//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------

static void on_client_data(ppdb_base_async_handle_t* handle, const uint8_t* data, size_t size);
static void on_client_close(ppdb_base_async_handle_t* handle);
static void on_new_connection(ppdb_base_async_handle_t* handle, int client_fd);

//-----------------------------------------------------------------------------
// Memory Management
//-----------------------------------------------------------------------------

static void* ppdb_base_alloc(size_t size) {
    void* ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

static void ppdb_base_free(void* ptr) {
    free(ptr);
}

//-----------------------------------------------------------------------------
// Server Implementation
//-----------------------------------------------------------------------------

// Create server instance
ppdb_error_t ppdb_server_create(struct ppdb_server** server, ppdb_ctx_t* ctx, ppdb_net_config_t* config) {
    if (!server || !ctx || !config) {
        return PPDB_ERR_PARAM;
    }
    
    // Allocate server
    struct ppdb_server* s = ppdb_base_alloc(sizeof(struct ppdb_server));
    if (!s) {
        return PPDB_ERR_MEMORY;
    }
    
    // Initialize server
    s->ctx = ctx;
    s->peer_ops = peer_get_ops(config->protocol);
    if (!s->peer_ops) {
        ppdb_base_free(s);
        return PPDB_ERR_PARAM;
    }
    
    // Create event loop
    s->loop = ppdb_base_async_loop_create();
    if (!s->loop) {
        ppdb_base_free(s);
        return PPDB_ERR_SYSTEM;
    }
    
    // Create listen handle
    s->listen_handle = ppdb_base_async_handle_create(s->loop);
    if (!s->listen_handle) {
        ppdb_base_async_loop_destroy(s->loop);
        ppdb_base_free(s);
        return PPDB_ERR_SYSTEM;
    }
    
    // Set callbacks
    ppdb_base_async_handle_set_accept_cb(s->listen_handle, on_new_connection);
    ppdb_base_async_handle_set_data_cb(s->listen_handle, NULL);
    ppdb_base_async_handle_set_close_cb(s->listen_handle, NULL);
    
    // Set user data
    ppdb_base_async_handle_set_data(s->listen_handle, s);
    
    // Listen
    ppdb_error_t err = ppdb_base_async_handle_listen(s->listen_handle, config->host, config->port);
    if (err != PPDB_OK) {
        ppdb_base_async_handle_destroy(s->listen_handle);
        ppdb_base_async_loop_destroy(s->loop);
        ppdb_base_free(s);
        return err;
    }
    
    *server = s;
    return PPDB_OK;
}

// Start server
ppdb_error_t ppdb_server_start(struct ppdb_server* server) {
    if (!server) {
        return PPDB_ERR_PARAM;
    }
    
    if (server->running) {
        return PPDB_ERR_BUSY;
    }
    
    server->running = true;
    return ppdb_base_async_loop_run(server->loop);
}

// Stop server
ppdb_error_t ppdb_server_stop(struct ppdb_server* server) {
    if (!server) {
        return PPDB_ERR_PARAM;
    }
    
    if (!server->running) {
        return PPDB_OK;
    }
    
    server->running = false;
    ppdb_base_async_loop_stop(server->loop);
    return PPDB_OK;
}

// Destroy server instance
void ppdb_server_destroy(struct ppdb_server* server) {
    if (!server) {
        return;
    }
    
    if (server->running) {
        ppdb_server_stop(server);
    }
    
    if (server->listen_handle) {
        ppdb_base_async_handle_destroy(server->listen_handle);
    }
    
    if (server->loop) {
        ppdb_base_async_loop_destroy(server->loop);
    }
    
    ppdb_base_free(server);
}

//-----------------------------------------------------------------------------
// Event Handlers
//-----------------------------------------------------------------------------

// Handle client data
static void on_client_data(ppdb_base_async_handle_t* handle, const uint8_t* data, size_t size) {
    client_state_t* state = (client_state_t*)ppdb_base_async_handle_get_data(handle);
    if (!state) {
        return;
    }
    
    // Create connection handle
    ppdb_handle_t conn = NULL;
    ppdb_error_t err = ppdb_conn_create(&conn, state->server->peer_ops, state->server->ctx);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to create connection handle\n");
        ppdb_base_async_handle_close(handle);
        return;
    }
    
    // Set socket
    err = ppdb_conn_set_socket(conn, ppdb_base_async_handle_get_fd(handle));
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to set connection socket\n");
        ppdb_conn_destroy(conn);
        ppdb_base_async_handle_close(handle);
        return;
    }
    
    // Handle data
    err = state->server->peer_ops->on_data(state->peer_data, conn, data, size);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to handle client data\n");
        ppdb_conn_destroy(conn);
        ppdb_base_async_handle_close(handle);
        return;
    }
    
    ppdb_conn_destroy(conn);
}

// Handle client close
static void on_client_close(ppdb_base_async_handle_t* handle) {
    client_state_t* state = (client_state_t*)ppdb_base_async_handle_get_data(handle);
    if (!state) {
        return;
    }
    
    // Notify protocol
    if (state->peer_data) {
        ppdb_handle_t conn = NULL;
        ppdb_error_t err = ppdb_conn_create(&conn, state->server->peer_ops, state->server->ctx);
        if (err == PPDB_OK) {
            state->server->peer_ops->on_disconnect(state->peer_data, conn);
            ppdb_conn_destroy(conn);
        }
        
        state->server->peer_ops->destroy(state->peer_data);
    }
    
    // Free state
    ppdb_base_free(state);
}

// Handle new connection
static void on_new_connection(ppdb_base_async_handle_t* handle, int client_fd) {
    struct ppdb_server* server = (struct ppdb_server*)ppdb_base_async_handle_get_data(handle);
    if (!server) {
        close(client_fd);
        return;
    }
    
    fprintf(stderr, "New client connected from fd %d\n", client_fd);
    
    // Create client handle
    ppdb_base_async_handle_t* client_handle = ppdb_base_async_handle_create(server->loop);
    if (!client_handle) {
        fprintf(stderr, "Failed to create client handle\n");
        close(client_fd);
        return;
    }
    
    // Create client state
    client_state_t* state = ppdb_base_alloc(sizeof(client_state_t));
    if (!state) {
        fprintf(stderr, "Failed to allocate client state\n");
        ppdb_base_async_handle_destroy(client_handle);
        close(client_fd);
        return;
    }
    
    // Initialize state
    state->server = server;
    state->handle = client_handle;
    
    // Create protocol instance
    ppdb_error_t err = server->peer_ops->create(&state->peer_data, server->ctx);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to create protocol instance\n");
        ppdb_base_free(state);
        ppdb_base_async_handle_destroy(client_handle);
        close(client_fd);
        return;
    }
    
    // Set callbacks
    ppdb_base_async_handle_set_data_cb(client_handle, on_client_data);
    ppdb_base_async_handle_set_close_cb(client_handle, on_client_close);
    
    // Set user data
    ppdb_base_async_handle_set_data(client_handle, state);
    
    // Accept connection
    err = ppdb_base_async_handle_accept(client_handle, client_fd);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to accept client connection\n");
        server->peer_ops->destroy(state->peer_data);
        ppdb_base_free(state);
        ppdb_base_async_handle_destroy(client_handle);
        close(client_fd);
        return;
    }
    
    // Notify protocol
    ppdb_handle_t conn = NULL;
    err = ppdb_conn_create(&conn, server->peer_ops, server->ctx);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to create connection handle\n");
        server->peer_ops->destroy(state->peer_data);
        ppdb_base_free(state);
        ppdb_base_async_handle_destroy(client_handle);
        return;
    }
    
    err = server->peer_ops->on_connect(state->peer_data, conn);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to handle client connection\n");
        ppdb_conn_destroy(conn);
        server->peer_ops->destroy(state->peer_data);
        ppdb_base_free(state);
        ppdb_base_async_handle_destroy(client_handle);
        return;
    }
    
    ppdb_conn_destroy(conn);
}

#endif // PPDB_PEER_SERVER_INC_C_ 