#include "peer_internal.h"

//-----------------------------------------------------------------------------
// Context Management
//-----------------------------------------------------------------------------

struct ppdb_peer_s {
    ppdb_ctx_t db_ctx;         // Database context
    ppdb_engine_t* engine;     // Storage engine (server only)
    ppdb_peer_mode_t mode;     // Peer mode
    ppdb_peer_connection_callback conn_cb;  // Connection callback
    void* user_data;           // User callback data
    bool running;              // Running flag
    
    // Network
    int listen_fd;             // Listen socket (server)
    ppdb_peer_connection_t* connections;  // Active connections
    size_t conn_count;         // Connection count
    
    // Threading
    ppdb_base_async_loop_t* loop;  // Event loop
    ppdb_base_thread_t* io_threads;  // IO threads
    size_t thread_count;       // Thread count
};

struct ppdb_peer_connection_s {
    ppdb_peer_t* peer;         // Owner peer
    int socket_fd;             // Connection socket
    bool connected;            // Connection state
    
    // Buffer management
    uint8_t* read_buf;         // Read buffer
    size_t read_size;          // Read buffer size
    uint8_t* write_buf;        // Write buffer  
    size_t write_size;         // Write buffer size
    
    // Request handling
    ppdb_peer_request_callback req_cb;  // Request callback
    void* req_data;            // Request user data
};

static ppdb_peer_connection_t* connection_create(ppdb_peer_t* peer, int fd) {
    ppdb_peer_connection_t* conn = ppdb_base_alloc(sizeof(ppdb_peer_connection_t));
    if (!conn) return NULL;
    
    memset(conn, 0, sizeof(ppdb_peer_connection_t));
    conn->peer = peer;
    conn->socket_fd = fd;
    
    // Allocate buffers
    conn->read_buf = ppdb_base_alloc(PPDB_PEER_BUFFER_SIZE);
    conn->write_buf = ppdb_base_alloc(PPDB_PEER_BUFFER_SIZE);
    if (!conn->read_buf || !conn->write_buf) {
        connection_destroy(conn);
        return NULL;
    }
    
    return conn;
}

static void connection_destroy(ppdb_peer_connection_t* conn) {
    if (!conn) return;
    
    if (conn->socket_fd >= 0) {
        close(conn->socket_fd);
    }
    
    ppdb_base_free(conn->read_buf);
    ppdb_base_free(conn->write_buf);
    ppdb_base_free(conn);
}

//-----------------------------------------------------------------------------
// Network Functions 
//-----------------------------------------------------------------------------

static ppdb_error_t create_server_socket(ppdb_peer_t* peer,
                                       const char* host,
                                       uint16_t port) {
    // Create socket
    peer->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (peer->listen_fd < 0) {
        return PPDB_ERR_NETWORK;
    }
    
    // Set options
    int opt = 1;
    setsockopt(peer->listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);
    
    if (bind(peer->listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(peer->listen_fd);
        return PPDB_ERR_NETWORK;
    }
    
    // Listen
    if (listen(peer->listen_fd, SOMAXCONN) < 0) {
        close(peer->listen_fd);
        return PPDB_ERR_NETWORK;
    }
    
    return PPDB_OK;
}

static void on_client_readable(ppdb_base_async_loop_t* loop,
                             int fd,
                             void* user_data) {
    ppdb_peer_connection_t* conn = user_data;
    
    // Read data
    ssize_t n = read(fd, conn->read_buf + conn->read_size,
                     PPDB_PEER_BUFFER_SIZE - conn->read_size);
    if (n <= 0) {
        // Connection closed or error
        if (conn->peer->conn_cb) {
            conn->peer->conn_cb(conn, PPDB_ERR_NETWORK, conn->peer->user_data);
        }
        connection_destroy(conn);
        return;
    }
    
    conn->read_size += n;
    
    // Process requests
    while (conn->read_size >= sizeof(ppdb_peer_request_t)) {
        ppdb_peer_request_t* req = (ppdb_peer_request_t*)conn->read_buf;
        
        // Handle request
        ppdb_peer_response_t resp = {0};
        if (conn->peer->mode == PPDB_PEER_MODE_SERVER && conn->peer->engine) {
            // Process using storage engine
            switch (req->type) {
                case PPDB_PEER_REQ_GET:
                    resp.error = ppdb_engine_get(conn->peer->engine,
                                               &req->key, &resp.value);
                    break;
                    
                case PPDB_PEER_REQ_SET:
                    resp.error = ppdb_engine_put(conn->peer->engine,
                                               &req->key, &req->value);
                    break;
                    
                case PPDB_PEER_REQ_DELETE:
                    resp.error = ppdb_engine_delete(conn->peer->engine,
                                                  &req->key);
                    break;
                    
                default:
                    resp.error = PPDB_ERR_INVALID;
            }
        }
        
        // Send response
        if (conn->req_cb) {
            conn->req_cb(conn, &resp, conn->req_data);
        }
        
        // Remove processed request
        size_t req_size = sizeof(ppdb_peer_request_t);
        memmove(conn->read_buf, conn->read_buf + req_size,
                conn->read_size - req_size);
        conn->read_size -= req_size;
    }
}

static void on_server_acceptable(ppdb_base_async_loop_t* loop,
                               int fd,
                               void* user_data) {
    ppdb_peer_t* peer = user_data;
    
    // Accept connection
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    int client_fd = accept(fd, (struct sockaddr*)&addr, &addr_len);
    if (client_fd < 0) {
        return;
    }
    
    // Create connection
    ppdb_peer_connection_t* conn = connection_create(peer, client_fd);
    if (!conn) {
        close(client_fd);
        return;
    }
    
    // Add to event loop
    ppdb_base_async_loop_add(loop, client_fd, conn,
                            on_client_readable, NULL);
    
    // Notify user
    if (peer->conn_cb) {
        peer->conn_cb(conn, PPDB_OK, peer->user_data);
    }
}

//-----------------------------------------------------------------------------
// Public Functions
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_peer_create(ppdb_peer_t** peer,
                             const ppdb_peer_config_t* config,
                             ppdb_engine_t* engine) {
    if (!peer || !config) {
        return PPDB_ERR_PARAM;
    }
    
    // Allocate peer
    ppdb_peer_t* p = ppdb_base_alloc(sizeof(ppdb_peer_t));
    if (!p) {
        return PPDB_ERR_MEMORY;
    }
    
    memset(p, 0, sizeof(ppdb_peer_t));
    p->mode = config->mode;
    p->engine = engine;
    
    // Create event loop
    p->loop = ppdb_base_async_loop_create();
    if (!p->loop) {
        ppdb_peer_destroy(p);
        return PPDB_ERR_MEMORY;
    }
    
    // Create IO threads
    p->thread_count = config->io_threads;
    p->io_threads = ppdb_base_alloc(sizeof(ppdb_base_thread_t) * p->thread_count);
    if (!p->io_threads) {
        ppdb_peer_destroy(p);
        return PPDB_ERR_MEMORY;
    }
    
    *peer = p;
    return PPDB_OK;
}

void ppdb_peer_destroy(ppdb_peer_t* peer) {
    if (!peer) return;
    
    ppdb_peer_stop(peer);
    
    if (peer->loop) {
        ppdb_base_async_loop_destroy(peer->loop);
    }
    
    ppdb_base_free(peer->io_threads);
    ppdb_base_free(peer);
}

ppdb_error_t ppdb_peer_start(ppdb_peer_t* peer) {
    if (!peer) {
        return PPDB_ERR_PARAM;
    }
    
    if (peer->running) {
        return PPDB_OK;
    }
    
    // Start IO threads
    for (size_t i = 0; i < peer->thread_count; i++) {
        ppdb_base_thread_create(&peer->io_threads[i], NULL, NULL);
    }
    
    // Start event loop
    ppdb_base_async_loop_run(peer->loop);
    
    peer->running = true;
    return PPDB_OK;
}

ppdb_error_t ppdb_peer_stop(ppdb_peer_t* peer) {
    if (!peer) {
        return PPDB_ERR_PARAM;
    }
    
    if (!peer->running) {
        return PPDB_OK;
    }
    
    // Stop event loop
    ppdb_base_async_loop_stop(peer->loop);
    
    // Stop IO threads
    for (size_t i = 0; i < peer->thread_count; i++) {
        ppdb_base_thread_join(&peer->io_threads[i]);
    }
    
    peer->running = false;
    return PPDB_OK;
}

ppdb_error_t ppdb_peer_set_connection_callback(ppdb_peer_t* peer,
                                             ppdb_peer_connection_callback cb,
                                             void* user_data) {
    if (!peer) {
        return PPDB_ERR_PARAM;
    }
    
    peer->conn_cb = cb;
    peer->user_data = user_data;
    
    return PPDB_OK;
}

ppdb_error_t ppdb_peer_connect(ppdb_peer_t* peer,
                              const char* host,
                              uint16_t port,
                              ppdb_peer_connection_t** conn) {
    if (!peer || !host || !conn) {
        return PPDB_ERR_PARAM;
    }
    
    if (peer->mode != PPDB_PEER_MODE_CLIENT) {
        return PPDB_ERR_INVALID;
    }
    
    // Create socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return PPDB_ERR_NETWORK;
    }
    
    // Connect
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &addr.sin_addr);
    
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return PPDB_ERR_NETWORK;
    }
    
    // Create connection
    ppdb_peer_connection_t* c = connection_create(peer, fd);
    if (!c) {
        close(fd);
        return PPDB_ERR_MEMORY;
    }
    
    // Add to event loop
    ppdb_base_async_loop_add(peer->loop, fd, c,
                            on_client_readable, NULL);
    
    *conn = c;
    return PPDB_OK;
}

ppdb_error_t ppdb_peer_disconnect(ppdb_peer_connection_t* conn) {
    if (!conn) {
        return PPDB_ERR_PARAM;
    }
    
    connection_destroy(conn);
    return PPDB_OK;
}

ppdb_error_t ppdb_peer_async_request(ppdb_peer_connection_t* conn,
                                    const ppdb_peer_request_t* req,
                                    ppdb_peer_request_callback cb,
                                    void* user_data) {
    if (!conn || !req || !cb) {
        return PPDB_ERR_PARAM;
    }
    
    // Set callback
    conn->req_cb = cb;
    conn->req_data = user_data;
    
    // Send request
    ssize_t n = write(conn->socket_fd, req, sizeof(ppdb_peer_request_t));
    if (n != sizeof(ppdb_peer_request_t)) {
        return PPDB_ERR_NETWORK;
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_peer_get_stats(ppdb_peer_t* peer,
                                char* buffer,
                                size_t size) {
    if (!peer || !buffer || size == 0) {
        return PPDB_ERR_PARAM;
    }
    
    // Format stats
    int n = snprintf(buffer, size,
                    "Mode: %s\n"
                    "Connections: %zu\n"
                    "IO Threads: %zu\n",
                    peer->mode == PPDB_PEER_MODE_SERVER ? "Server" : "Client",
                    peer->conn_count,
                    peer->thread_count);
    
    if (n < 0 || (size_t)n >= size) {
        return PPDB_ERR_BUFFER;
    }
    
    return PPDB_OK;
}
