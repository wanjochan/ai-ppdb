/*
 * base_net.inc.c - Network Infrastructure Implementation
 */

#include <cosmopolitan.h>
#include "internal/base.h"

//-----------------------------------------------------------------------------
// Event Loop Implementation
//-----------------------------------------------------------------------------

ppdb_error_t ppdb_base_event_loop_create(ppdb_base_event_loop_t** loop) {
    if (!loop) return PPDB_ERR_PARAM;
    
    ppdb_base_event_loop_t* new_loop = NULL;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_base_event_loop_t), (void**)&new_loop);
    if (err != PPDB_OK) return err;
    
    new_loop->running = false;
    new_loop->handlers = NULL;
    new_loop->handler_count = 0;
    new_loop->lock = NULL;
    new_loop->epoll_fd = -1;
    new_loop->kqueue_fd = NULL;
    new_loop->iocp_handle = NULL;
    
    *loop = new_loop;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_event_loop_destroy(ppdb_base_event_loop_t* loop) {
    if (!loop) return PPDB_ERR_PARAM;
    
    // Stop the loop if it's running
    loop->running = false;
    
    // Free all handlers
    ppdb_base_event_handler_t* handler = loop->handlers;
    while (handler) {
        ppdb_base_event_handler_t* next = handler->next;
        ppdb_base_mem_free(handler);
        handler = next;
    }
    
    // Clean up platform-specific resources
    if (loop->epoll_fd >= 0) {
        close(loop->epoll_fd);
    }
    if (loop->kqueue_fd) {
        close((int)(size_t)loop->kqueue_fd);
    }
    if (loop->iocp_handle) {
        CloseHandle((int64_t)(size_t)loop->iocp_handle);
    }
    
    ppdb_base_mem_free(loop);
    return PPDB_OK;
}

ppdb_error_t ppdb_base_event_handler_add(ppdb_base_event_loop_t* loop, ppdb_base_event_handler_t* handler) {
    if (!loop || !handler) return PPDB_ERR_PARAM;
    
    handler->next = loop->handlers;
    loop->handlers = handler;
    loop->handler_count++;
    
    return PPDB_OK;
}

ppdb_error_t ppdb_base_event_handler_remove(ppdb_base_event_loop_t* loop, ppdb_base_event_handler_t* handler) {
    if (!loop || !handler) return PPDB_ERR_PARAM;
    
    ppdb_base_event_handler_t** curr = &loop->handlers;
    while (*curr) {
        if (*curr == handler) {
            *curr = handler->next;
            loop->handler_count--;
            ppdb_base_mem_free(handler);
            return PPDB_OK;
        }
        curr = &(*curr)->next;
    }
    
    return PPDB_ERR_NOT_FOUND;
}

ppdb_error_t ppdb_base_event_loop_run(ppdb_base_event_loop_t* loop, int timeout_ms) {
    if (!loop) return PPDB_ERR_PARAM;
    
    loop->running = true;
    
    while (loop->running) {
        // TODO: Implement event polling using select/epoll/kqueue
        ppdb_base_sleep(1);  // Temporary: sleep to avoid busy loop
    }
    
    return PPDB_OK;
}

// Forward declarations
static ppdb_error_t create_connection(ppdb_net_server_t* server, int client_fd, ppdb_connection_t** out_conn);
static void handle_connection_event(ppdb_base_event_handler_t* handler, uint32_t events);
static void io_thread_func(void* arg);

// Create a new connection
static ppdb_error_t create_connection(ppdb_net_server_t* server, int client_fd, ppdb_connection_t** out_conn) {
    ppdb_connection_t* new_conn = NULL;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_connection_t), (void**)&new_conn);
    if (err != PPDB_OK) return err;

    // Initialize basic fields
    new_conn->fd = client_fd;
    new_conn->server = server;
    new_conn->recv_buffer = NULL;
    new_conn->recv_size = 0;
    new_conn->buffer_size = 0;

    // Initialize state fields
    new_conn->state = PPDB_CONN_STATE_INIT;
    err = ppdb_base_time_get_microseconds(&new_conn->last_active_time);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(new_conn);
        return err;
    }
    new_conn->connect_time = (uint32_t)(new_conn->last_active_time / 1000); // Convert to milliseconds
    new_conn->idle_timeout = 60000; // Default 60 seconds timeout

    // Initialize statistics
    new_conn->bytes_received = 0;
    new_conn->bytes_sent = 0;
    new_conn->request_count = 0;
    new_conn->error_count = 0;

    *out_conn = new_conn;
    return PPDB_OK;
}

// Handle read events
static ppdb_error_t handle_read(ppdb_connection_t* conn) {
    char buf[4096];
    ssize_t n = read(conn->fd, buf, sizeof(buf));
    
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return PPDB_OK;
        }
        conn->error_count++;
        return PPDB_ERR_IO;
    }
    
    if (n == 0) {
        return PPDB_ERR_CLOSED;
    }
    
    // Update statistics and activity time
    conn->bytes_received += n;
    ppdb_base_time_get_microseconds(&conn->last_active_time);
    conn->state = PPDB_CONN_STATE_ACTIVE;
    
    // Ensure buffer capacity
    size_t required = conn->recv_size + n;
    if (required > conn->buffer_size) {
        size_t new_size = conn->buffer_size ? conn->buffer_size * 2 : 4096;
        while (new_size < required) new_size *= 2;
        
        void* new_buf = NULL;
        ppdb_error_t err = ppdb_base_mem_malloc(new_size, &new_buf);
        if (err != PPDB_OK) {
            conn->error_count++;
            return err;
        }
        
        if (conn->recv_buffer) {
            memcpy(new_buf, conn->recv_buffer, conn->recv_size);
            ppdb_base_mem_free(conn->recv_buffer);
        }
        
        conn->recv_buffer = new_buf;
        conn->buffer_size = new_size;
    }
    
    // Append data
    memcpy(conn->recv_buffer + conn->recv_size, buf, n);
    conn->recv_size += n;
    conn->request_count++;
    
    return PPDB_OK;
}

// Network server functions
ppdb_error_t ppdb_base_net_server_create(ppdb_net_server_t** out_server) {
    if (!out_server) return PPDB_ERR_PARAM;
    
    ppdb_net_server_t* new_server = NULL;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_net_server_t), (void**)&new_server);
    if (err != PPDB_OK) return err;
    
    // Initialize server
    new_server->listen_fd = -1;
    new_server->running = false;
    new_server->io_threads = NULL;
    new_server->thread_count = 0;
    new_server->user_data = NULL;
    
    // Create event loop
    err = ppdb_base_event_loop_create(&new_server->event_loop);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(new_server);
        return err;
    }
    
    *out_server = new_server;
    return PPDB_OK;
}

ppdb_error_t ppdb_base_net_server_start(ppdb_net_server_t* server) {
    if (!server) return PPDB_ERR_PARAM;
    
    // Create socket
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) return PPDB_ERR_IO;
    
    // Set non-blocking
    int flags = fcntl(server->listen_fd, F_GETFL, 0);
    fcntl(server->listen_fd, F_SETFL, flags | O_NONBLOCK);
    
    // Start IO threads
    server->running = true;
    server->thread_count = PPDB_IO_DEFAULT_THREADS;
    
    server->io_threads = malloc(sizeof(ppdb_base_thread_t*) * server->thread_count);
    if (!server->io_threads) {
        close(server->listen_fd);
        return PPDB_ERR_MEMORY;
    }
    
    for (size_t i = 0; i < server->thread_count; i++) {
        ppdb_error_t err = ppdb_base_thread_create(&server->io_threads[i], io_thread_func, server);
        if (err != PPDB_OK) {
            server->running = false;
            for (size_t j = 0; j < i; j++) {
                ppdb_base_thread_join(server->io_threads[j]);
                ppdb_base_thread_destroy(server->io_threads[j]);
            }
            free(server->io_threads);
            close(server->listen_fd);
            return err;
        }
    }
    
    return PPDB_OK;
}

ppdb_error_t ppdb_base_net_server_stop(ppdb_net_server_t* server) {
    if (!server) return PPDB_ERR_PARAM;
    
    server->running = false;
    
    // Wait for IO threads
    for (size_t i = 0; i < server->thread_count; i++) {
        ppdb_base_thread_join(server->io_threads[i]);
        ppdb_base_thread_destroy(server->io_threads[i]);
    }
    
    free(server->io_threads);
    close(server->listen_fd);
    
    return PPDB_OK;
}

ppdb_error_t ppdb_base_net_server_destroy(ppdb_net_server_t* server) {
    if (!server) return PPDB_ERR_PARAM;
    
    ppdb_base_event_loop_destroy(server->event_loop);
    ppdb_base_mem_free(server);
    
    return PPDB_OK;
}

// IO thread function
static void io_thread_func(void* arg) {
    ppdb_net_server_t* server = (ppdb_net_server_t*)arg;
    
    while (server->running) {
        // Accept new connections
        int client_fd = accept(server->listen_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            break;
        }
        
        // Set non-blocking
        int flags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
        
        // Create connection
        ppdb_connection_t* conn = NULL;
        ppdb_error_t err = create_connection(server, client_fd, &conn);
        if (err != PPDB_OK) {
            close(client_fd);
            continue;
        }
    }
}

// Check connection timeout
static ppdb_error_t check_connection_timeout(ppdb_connection_t* conn) {
    if (!conn) return PPDB_ERR_PARAM;
    
    uint64_t now;
    ppdb_error_t err = ppdb_base_time_get_microseconds(&now);
    if (err != PPDB_OK) return err;
    
    // Convert to milliseconds
    uint64_t idle_time = (now - conn->last_active_time) / 1000;
    
    if (idle_time >= conn->idle_timeout) {
        conn->state = PPDB_CONN_STATE_CLOSING;
        return PPDB_OK;
    }
    
    return PPDB_OK;
}

// Connection cleanup
static ppdb_error_t cleanup_connection(ppdb_connection_t* conn) {
    if (!conn) return PPDB_ERR_PARAM;
    
    // Close socket
    if (conn->fd >= 0) {
        close(conn->fd);
        conn->fd = -1;
    }
    
    // Free receive buffer
    if (conn->recv_buffer) {
        ppdb_base_mem_free(conn->recv_buffer);
        conn->recv_buffer = NULL;
    }
    
    conn->state = PPDB_CONN_STATE_CLOSED;
    return PPDB_OK;
}

// Handle connection event
static ppdb_error_t handle_connection_event(ppdb_connection_t* conn) {
    if (!conn) return PPDB_ERR_PARAM;
    
    // Check timeout first
    ppdb_error_t err = check_connection_timeout(conn);
    if (err != PPDB_OK) return err;
    
    if (conn->state == PPDB_CONN_STATE_CLOSING) {
        return cleanup_connection(conn);
    }
    
    // Handle read event
    err = handle_read(conn);
    if (err != PPDB_OK) {
        conn->error_count++;
        return err;
    }
    
    // Update activity time
    err = ppdb_base_time_get_microseconds(&conn->last_active_time);
    if (err != PPDB_OK) return err;
    
    conn->state = PPDB_CONN_STATE_ACTIVE;
    return PPDB_OK;
}

// Create connection
ppdb_error_t ppdb_base_connection_create(ppdb_connection_t** conn, int fd) {
    if (!conn || fd < 0) return PPDB_ERR_PARAM;
    
    ppdb_connection_t* new_conn = NULL;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_connection_t), (void**)&new_conn);
    if (err != PPDB_OK) return err;
    
    // Initialize basic fields
    new_conn->fd = fd;
    new_conn->server = NULL;
    new_conn->recv_buffer = NULL;
    new_conn->recv_size = 0;
    new_conn->buffer_size = 0;
    
    // Initialize new fields
    new_conn->state = PPDB_CONN_STATE_INIT;
    err = ppdb_base_time_get_microseconds(&new_conn->last_active_time);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(new_conn);
        return err;
    }
    new_conn->connect_time = new_conn->last_active_time;
    new_conn->idle_timeout = 60000; // Default 60s timeout
    
    // Initialize statistics
    new_conn->bytes_received = 0;
    new_conn->bytes_sent = 0;
    new_conn->request_count = 0;
    new_conn->error_count = 0;
    
    *conn = new_conn;
    return PPDB_OK;
}

// Create connection
static ppdb_error_t create_connection(ppdb_net_server_t* server, int client_fd, ppdb_connection_t** conn) {
    if (!server || client_fd < 0 || !conn) return PPDB_ERR_PARAM;
    
    ppdb_connection_t* new_conn = NULL;
    ppdb_error_t err = ppdb_base_mem_malloc(sizeof(ppdb_connection_t), (void**)&new_conn);
    if (err != PPDB_OK) return err;
    
    // Initialize connection
    new_conn->fd = client_fd;
    new_conn->server = server;
    new_conn->recv_buffer = NULL;
    new_conn->recv_size = 0;
    new_conn->buffer_size = PPDB_DEFAULT_BUFFER_SIZE;
    
    // Initialize new fields
    new_conn->state = PPDB_CONN_STATE_INIT;
    ppdb_base_time_get_microseconds(&new_conn->last_active_time);
    new_conn->idle_timeout = PPDB_DEFAULT_IDLE_TIMEOUT;
    new_conn->connect_time = (uint32_t)(new_conn->last_active_time / 1000000);
    
    // Initialize statistics
    new_conn->bytes_received = 0;
    new_conn->bytes_sent = 0;
    new_conn->request_count = 0;
    new_conn->error_count = 0;
    
    // Allocate receive buffer
    err = ppdb_base_mem_malloc(new_conn->buffer_size, &new_conn->recv_buffer);
    if (err != PPDB_OK) {
        ppdb_base_mem_free(new_conn);
        return err;
    }
    
    *conn = new_conn;
    return PPDB_OK;
}

// Handle read event
static ppdb_error_t handle_read(ppdb_connection_t* conn) {
    if (!conn || conn->fd < 0) return PPDB_ERR_PARAM;
    
    // Read data
    ssize_t bytes = read(conn->fd, 
                        (char*)conn->recv_buffer + conn->recv_size,
                        conn->buffer_size - conn->recv_size);
    
    if (bytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return PPDB_OK;
        }
        conn->error_count++;
        return PPDB_ERR_IO;
    }
    
    if (bytes == 0) {
        return PPDB_ERR_CLOSED;
    }
    
    // Update statistics
    conn->bytes_received += bytes;
    conn->recv_size += bytes;
    conn->request_count++;
    
    // Update activity time
    ppdb_base_time_get_microseconds(&conn->last_active_time);
    conn->state = PPDB_CONN_STATE_ACTIVE;
    
    return PPDB_OK;
}

// Get connection statistics
ppdb_error_t ppdb_net_get_connection_stats(ppdb_connection_t* conn,
                                         uint64_t* bytes_received,
                                         uint64_t* bytes_sent,
                                         uint32_t* request_count,
                                         uint32_t* error_count,
                                         uint32_t* uptime) {
    if (!conn) return PPDB_ERR_PARAM;
    
    if (bytes_received) *bytes_received = conn->bytes_received;
    if (bytes_sent) *bytes_sent = conn->bytes_sent;
    if (request_count) *request_count = conn->request_count;
    if (error_count) *error_count = conn->error_count;
    
    if (uptime) {
        uint64_t now;
        ppdb_base_time_get_microseconds(&now);
        *uptime = (uint32_t)((now / 1000000) - conn->connect_time);
    }
    
    return PPDB_OK;
}

// Set connection timeout
ppdb_error_t ppdb_net_set_connection_timeout(ppdb_connection_t* conn, uint32_t timeout_ms) {
    if (!conn) return PPDB_ERR_PARAM;
    if (timeout_ms == 0) return PPDB_ERR_PARAM;
    
    conn->idle_timeout = timeout_ms;
    return PPDB_OK;
}

// Get connection state
ppdb_error_t ppdb_net_get_connection_state(ppdb_connection_t* conn, ppdb_connection_state_t* state) {
    if (!conn || !state) return PPDB_ERR_PARAM;
    
    *state = conn->state;
    return PPDB_OK;
} 