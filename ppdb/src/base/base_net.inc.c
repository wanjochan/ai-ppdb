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

    new_conn->fd = client_fd;
    new_conn->server = server;
    new_conn->recv_buffer = NULL;
    new_conn->recv_size = 0;
    new_conn->buffer_size = 0;

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
        return PPDB_ERR_IO;
    }
    
    if (n == 0) {
        return PPDB_ERR_CLOSED;
    }
    
    // Ensure buffer capacity
    size_t required = conn->recv_size + n;
    if (required > conn->buffer_size) {
        size_t new_size = conn->buffer_size ? conn->buffer_size * 2 : 4096;
        while (new_size < required) new_size *= 2;
        
        void* new_buf = NULL;
        ppdb_error_t err = ppdb_base_mem_malloc(new_size, &new_buf);
        if (err != PPDB_OK) return err;
        
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

// Handle connection events
static void handle_connection_event(ppdb_base_event_handler_t* h, uint32_t events) {
    ppdb_connection_t* conn = (ppdb_connection_t*)h->user_data;
    
    if (events & PPDB_EVENT_READ) {
        ppdb_error_t err = handle_read(conn);
        if (err != PPDB_OK) {
            ppdb_base_event_handler_remove(conn->server->event_loop, h);
            close(conn->fd);
            ppdb_base_mem_free(conn->recv_buffer);
            ppdb_base_mem_free(conn);
            ppdb_base_mem_free(h);
            return;
        }
    }
} 