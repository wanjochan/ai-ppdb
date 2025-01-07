#ifndef PPDB_PEER_SERVER_INC_C_
#define PPDB_PEER_SERVER_INC_C_

#include <cosmopolitan.h>
#include "../internal/peer.h"
#include "../internal/base.h"
#include "../internal/storage.h"

//-----------------------------------------------------------------------------
// Server Context Implementation
//-----------------------------------------------------------------------------

struct ppdb_server_s {
    ppdb_ctx_t ctx;              // Database context
    ppdb_net_config_t config;    // Network configuration
    ppdb_handle_t peer;          // Network peer handle
    bool running;                // Server running flag
    int listen_fd;               // Listening socket
    pthread_t accept_thread;     // Accept thread handle
};

// Forward declarations
static void* accept_thread_func(void* arg);

//-----------------------------------------------------------------------------
// Server Management Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_server_create(ppdb_server_t* server, ppdb_ctx_t ctx, const ppdb_net_config_t* config) {
    if (!server || !ctx || !config) {
        return PPDB_ERR_PARAM;
    }

    // Allocate server context
    ppdb_server_t srv = (ppdb_server_t)malloc(sizeof(struct ppdb_server_s));
    if (!srv) {
        return PPDB_ERR_MEMORY;
    }
    memset(srv, 0, sizeof(struct ppdb_server_s));

    // Initialize server context
    srv->ctx = ctx;
    srv->config = *config;
    srv->running = false;
    srv->listen_fd = -1;

    *server = srv;
    return PPDB_OK;
}

ppdb_error_t ppdb_server_start(ppdb_server_t server) {
    if (!server) {
        fprintf(stderr, "Server context is null\n");
        return PPDB_ERR_PARAM;
    }

    if (server->running) {
        fprintf(stderr, "Server is already running\n");
        return PPDB_ERR_BUSY;
    }

    fprintf(stderr, "Creating socket...\n");
    // Create listening socket
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) {
        fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));
        return PPDB_ERR_NETWORK;
    }
    fprintf(stderr, "Socket created: fd=%d\n", server->listen_fd);

    // Set socket options
    int reuse = 1;
    if (setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
        fprintf(stderr, "Failed to set socket options: %s\n", strerror(errno));
        close(server->listen_fd);
        return PPDB_ERR_NETWORK;
    }
    fprintf(stderr, "Socket options set\n");

    // Bind socket
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server->config.port);
    addr.sin_addr.s_addr = inet_addr(server->config.host);

    fprintf(stderr, "Binding to %s:%d...\n", server->config.host, server->config.port);
    if (bind(server->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Failed to bind socket: %s\n", strerror(errno));
        close(server->listen_fd);
        return PPDB_ERR_NETWORK;
    }
    fprintf(stderr, "Socket bound successfully\n");

    // Listen
    fprintf(stderr, "Starting to listen...\n");
    if (listen(server->listen_fd, SOMAXCONN) < 0) {
        fprintf(stderr, "Failed to listen on socket: %s\n", strerror(errno));
        close(server->listen_fd);
        return PPDB_ERR_NETWORK;
    }
    fprintf(stderr, "Listening started\n");

    // Create peer instance
    fprintf(stderr, "Creating peer instance...\n");
    ppdb_error_t err = ppdb_conn_create(&server->peer, peer_get_memcached(), server->ctx);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to create peer instance: %d\n", err);
        close(server->listen_fd);
        return err;
    }
    fprintf(stderr, "Peer instance created\n");

    // Start peer with the listening socket
    fprintf(stderr, "Setting peer socket...\n");
    err = ppdb_conn_set_socket(server->peer, server->listen_fd);
    if (err != PPDB_OK) {
        fprintf(stderr, "Failed to set peer socket: %d\n", err);
        close(server->listen_fd);
        ppdb_conn_destroy(server->peer);
        server->peer = 0;
        return err;
    }
    fprintf(stderr, "Peer socket set\n");

    // Start accept thread
    fprintf(stderr, "Starting accept thread...\n");
    server->running = true;
    if (pthread_create(&server->accept_thread, NULL, accept_thread_func, server) != 0) {
        fprintf(stderr, "Failed to create accept thread: %s\n", strerror(errno));
        close(server->listen_fd);
        ppdb_conn_destroy(server->peer);
        server->peer = 0;
        server->running = false;
        return PPDB_ERR_NETWORK;
    }
    fprintf(stderr, "Accept thread started\n");

    fprintf(stderr, "Server successfully started and listening on %s:%d\n", 
            server->config.host, server->config.port);
    return PPDB_OK;
}

static void* accept_thread_func(void* arg) {
    ppdb_server_t server = (ppdb_server_t)arg;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    fprintf(stderr, "Accept thread running\n");

    while (server->running) {
        // Accept new connection
        fprintf(stderr, "Waiting for new connection...\n");
        int client_fd = accept(server->listen_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (server->running) {
                fprintf(stderr, "Failed to accept connection: %s\n", strerror(errno));
            }
            continue;
        }

        // Create new peer connection
        fprintf(stderr, "Creating peer for new connection...\n");
        ppdb_handle_t client_peer;
        ppdb_error_t err = ppdb_conn_create(&client_peer, peer_get_memcached(), server->ctx);
        if (err != PPDB_OK) {
            fprintf(stderr, "Failed to create peer for new connection: %d\n", err);
            close(client_fd);
            continue;
        }

        // Set socket for the new connection
        fprintf(stderr, "Setting socket for new connection...\n");
        err = ppdb_conn_set_socket(client_peer, client_fd);
        if (err != PPDB_OK) {
            fprintf(stderr, "Failed to set socket for new connection: %d\n", err);
            close(client_fd);
            ppdb_conn_destroy(client_peer);
            continue;
        }

        fprintf(stderr, "New client connected from %s:%d\n", 
                inet_ntoa(client_addr.sin_addr), 
                ntohs(client_addr.sin_port));
    }

    fprintf(stderr, "Accept thread exiting\n");
    return NULL;
}

ppdb_error_t ppdb_server_stop(ppdb_server_t server) {
    if (!server) {
        return PPDB_ERR_PARAM;
    }

    if (!server->running) {
        return PPDB_ERR_BUSY;
    }

    // Stop accept thread
    server->running = false;
    shutdown(server->listen_fd, SHUT_RDWR);
    pthread_join(server->accept_thread, NULL);

    // Close listening socket
    if (server->listen_fd >= 0) {
        close(server->listen_fd);
        server->listen_fd = -1;
    }

    // Stop peer
    ppdb_conn_close(server->peer);

    // Cleanup peer
    ppdb_conn_destroy(server->peer);
    server->peer = 0;

    return PPDB_OK;
}

ppdb_error_t ppdb_server_destroy(ppdb_server_t server) {
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

#endif // PPDB_PEER_SERVER_INC_C_ 