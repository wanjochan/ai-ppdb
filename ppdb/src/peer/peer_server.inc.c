#ifndef PPDB_PEER_SERVER_INC_C_
#define PPDB_PEER_SERVER_INC_C_

#include <cosmopolitan.h>
#include "../internal/peer.h"
#include "../internal/engine.h"

//-----------------------------------------------------------------------------
// Server Types
//-----------------------------------------------------------------------------

// Client state
typedef struct client_state {
    void* peer_data;                    // Protocol-specific data
    struct ppdb_server* server;         // Server instance
    ppdb_engine_async_handle_t* handle; // Client handle
} client_state_t;

// Server structure
struct ppdb_server {
    ppdb_ctx_t* ctx;                    // Database context
    const peer_ops_t* peer_ops;         // Protocol operations
    ppdb_engine_async_loop_t* loop;     // Event loop
    ppdb_engine_async_handle_t* listen_handle; // Listen handle
    bool running;                       // Server state
};

//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------

static void on_client_data(ppdb_engine_async_handle_t* handle, const uint8_t* data, size_t size);
static void on_client_close(ppdb_engine_async_handle_t* handle);
static void on_new_connection(ppdb_engine_async_handle_t* handle, int client_fd);

//-----------------------------------------------------------------------------
// Memory Management
//-----------------------------------------------------------------------------

static void* ppdb_server_alloc(size_t size) {
    void* ptr = ppdb_engine_malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

static void ppdb_server_free(void* ptr) {
    ppdb_engine_free(ptr);
}

//-----------------------------------------------------------------------------
// Server Implementation
//-----------------------------------------------------------------------------

// Create server instance
ppdb_error_t ppdb_server_create(ppdb_ctx_t* ctx, const peer_ops_t* ops, struct ppdb_server** server) {
    if (!ctx || !ops || !server) {
        return PPDB_ERR_PARAM;
    }
    
    // Allocate server structure
    *server = ppdb_server_alloc(sizeof(struct ppdb_server));
    if (!*server) {
        return PPDB_ERR_MEMORY;
    }
    
    // Initialize server
    (*server)->ctx = ctx;
    (*server)->peer_ops = ops;
    (*server)->running = false;
    
    // Create event loop
    ppdb_error_t err = ppdb_engine_async_loop_create(&(*server)->loop);
    if (err != PPDB_OK) {
        ppdb_server_free(*server);
        *server = NULL;
        return err;
    }
    
    // Create listen handle
    err = ppdb_engine_async_handle_create((*server)->loop, &(*server)->listen_handle);
    if (err != PPDB_OK) {
        ppdb_engine_async_loop_destroy((*server)->loop);
        ppdb_server_free(*server);
        *server = NULL;
        return err;
    }
    
    // Set callbacks
    ppdb_engine_async_handle_set_connection_cb((*server)->listen_handle, on_new_connection);
    
    // Set user data
    ppdb_engine_async_handle_set_data((*server)->listen_handle, *server);
    
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
    return ppdb_engine_async_loop_run(server->loop);
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
    ppdb_engine_async_loop_stop(server->loop);
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
        ppdb_engine_async_handle_destroy(server->listen_handle);
    }
    
    if (server->loop) {
        ppdb_engine_async_loop_destroy(server->loop);
    }
    
    ppdb_server_free(server);
}

//-----------------------------------------------------------------------------
// Event Handlers
//-----------------------------------------------------------------------------

// Handle client data
static void on_client_data(ppdb_engine_async_handle_t* handle, const uint8_t* data, size_t size) {
    client_state_t* state = (client_state_t*)ppdb_engine_async_handle_get_data(handle);
    if (!state) {
        return;
    }
    
    // Create connection handle
    ppdb_handle_t conn = NULL;
    ppdb_error_t err = ppdb_conn_create(&conn, state->server->peer_ops, state->server->ctx);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to create connection handle\n");
        ppdb_engine_async_handle_close(handle);
        return;
    }
    
    // Set socket
    err = ppdb_conn_set_socket(conn, ppdb_engine_async_handle_get_fd(handle));
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to set connection socket\n");
        ppdb_conn_destroy(conn);
        ppdb_engine_async_handle_close(handle);
        return;
    }
    
    // Handle data
    err = state->server->peer_ops->on_data(state->peer_data, conn, data, size);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to handle client data\n");
        ppdb_conn_destroy(conn);
        ppdb_engine_async_handle_close(handle);
        return;
    }
    
    ppdb_conn_destroy(conn);
}

// Handle client close
static void on_client_close(ppdb_engine_async_handle_t* handle) {
    client_state_t* state = (client_state_t*)ppdb_engine_async_handle_get_data(handle);
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
    ppdb_server_free(state);
}

// Handle new connection
static void on_new_connection(ppdb_engine_async_handle_t* handle, int client_fd) {
    struct ppdb_server* server = (struct ppdb_server*)ppdb_engine_async_handle_get_data(handle);
    if (!server) {
        close(client_fd);
        return;
    }
    
    fprintf(stderr, "New client connected from fd %d\n", client_fd);
    
    // Create client handle
    ppdb_engine_async_handle_t* client_handle = NULL;
    ppdb_error_t err = ppdb_engine_async_handle_create(server->loop, &client_handle);
    if (!client_handle) {
        fprintf(stderr, "Failed to create client handle\n");
        close(client_fd);
        return;
    }
    
    // Create client state
    client_state_t* state = ppdb_server_alloc(sizeof(client_state_t));
    if (!state) {
        fprintf(stderr, "Failed to allocate client state\n");
        ppdb_engine_async_handle_destroy(client_handle);
        close(client_fd);
        return;
    }
    
    // Initialize state
    state->server = server;
    state->handle = client_handle;
    
    // Create protocol instance
    err = server->peer_ops->create(&state->peer_data, server->ctx);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to create protocol instance\n");
        ppdb_server_free(state);
        ppdb_engine_async_handle_destroy(client_handle);
        close(client_fd);
        return;
    }
    
    // Set callbacks
    ppdb_engine_async_handle_set_data_cb(client_handle, on_client_data);
    ppdb_engine_async_handle_set_close_cb(client_handle, on_client_close);
    
    // Set user data
    ppdb_engine_async_handle_set_data(client_handle, state);
    
    // Accept connection
    err = ppdb_engine_async_handle_accept(client_handle, client_fd);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to accept client connection\n");
        server->peer_ops->destroy(state->peer_data);
        ppdb_server_free(state);
        ppdb_engine_async_handle_destroy(client_handle);
        close(client_fd);
        return;
    }
    
    // Notify protocol
    ppdb_handle_t conn = NULL;
    err = ppdb_conn_create(&conn, server->peer_ops, server->ctx);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to create connection handle\n");
        server->peer_ops->destroy(state->peer_data);
        ppdb_server_free(state);
        ppdb_engine_async_handle_destroy(client_handle);
        return;
    }
    
    err = server->peer_ops->on_connect(state->peer_data, conn);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to handle client connection\n");
        ppdb_conn_destroy(conn);
        server->peer_ops->destroy(state->peer_data);
        ppdb_server_free(state);
        ppdb_engine_async_handle_destroy(client_handle);
        return;
    }
    
    ppdb_conn_destroy(conn);
}

#endif // PPDB_PEER_SERVER_INC_C_ 