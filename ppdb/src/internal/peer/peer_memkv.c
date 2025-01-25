#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_platform.h"
#include "internal/peer/peer_memkv.h"
#include "internal/peer/peer_memkv_cmd.h"
#include "internal/poly/poly_cmdline.h"

//-----------------------------------------------------------------------------
// Command Line Options
//-----------------------------------------------------------------------------

const poly_cmd_option_t memkv_options[] = {
    {
        .name = "port",
        .desc = "Port to listen on",
        .has_value = true,
    },
    {
        .name = "start",
        .desc = "Start the service",
        .has_value = false,
    },
    {
        .name = "stop",
        .desc = "Stop the service",
        .has_value = false,
    },
    {
        .name = "status",
        .desc = "Show service status",
        .has_value = false,
    },
};

const int memkv_option_count = sizeof(memkv_options) / sizeof(memkv_options[0]);

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

memkv_context_t g_context = {0};

//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------

static infra_error_t create_listener(void);
static infra_error_t create_connection(infra_socket_t sock, memkv_conn_t** conn);
static void destroy_connection(memkv_conn_t* conn);
static void* handle_connection(void* arg);

//-----------------------------------------------------------------------------
// Service Management
//-----------------------------------------------------------------------------

infra_error_t memkv_init(uint16_t port, const infra_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Initialize global context
    memset(&g_context, 0, sizeof(g_context));
    g_context.port = port;

    // Initialize storage
    infra_error_t err = memkv_cmd_init();
    if (err != INFRA_OK) {
        return err;
    }

    // Create thread pool configuration
    infra_thread_pool_config_t pool_config = {
        .min_threads = MEMKV_MIN_THREADS,
        .max_threads = MEMKV_MAX_THREADS,
        .queue_size = MEMKV_QUEUE_SIZE,
        .idle_timeout = MEMKV_IDLE_TIMEOUT
    };

    // Create thread pool
    err = infra_thread_pool_create(&pool_config, &g_context.pool);
    if (err != INFRA_OK) {
        memkv_cmd_cleanup();
        return err;
    }

    g_context.start_time = time(NULL);
    return INFRA_OK;
}

infra_error_t memkv_cleanup(void) {
    if (g_context.is_running) {
        memkv_stop();
    }

    if (g_context.pool) {
        infra_thread_pool_destroy(g_context.pool);
        g_context.pool = NULL;
    }

    if (g_context.listen_sock) {
        infra_net_close(g_context.listen_sock);
        g_context.listen_sock = NULL;
    }

    return memkv_cmd_cleanup();
}

static infra_error_t create_listener(void) {
    // Create listen socket
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    infra_socket_t listener = NULL;
    infra_error_t err = infra_net_create(&listener, false, &config);
    if (err != INFRA_OK) {
        return err;
    }

    // Set socket options
    err = infra_net_set_reuseaddr(listener, true);
    if (err != INFRA_OK) {
        infra_net_close(listener);
        return err;
    }

    // Bind address
    infra_net_addr_t addr = {
        .host = "127.0.0.1",
        .port = g_context.port
    };
    
    err = infra_net_bind(listener, &addr);
    if (err != INFRA_OK) {
        infra_net_close(listener);
        return err;
    }

    err = infra_net_listen(listener);
    if (err != INFRA_OK) {
        infra_net_close(listener);
        return err;
    }

    g_context.listen_sock = listener;
    return INFRA_OK;
}

infra_error_t memkv_start(void) {
    if (g_context.is_running) {
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    // Create listener
    infra_error_t err = create_listener();
    if (err != INFRA_OK) {
        return err;
    }

    // Set non-blocking mode
    err = infra_net_set_nonblock(g_context.listen_sock, true);
    if (err != INFRA_OK) {
        infra_net_close(g_context.listen_sock);
        g_context.listen_sock = NULL;
        return err;
    }

    g_context.is_running = true;
    infra_printf("MemKV service started on port %d\n", g_context.port);
    
    while (g_context.is_running) {
        infra_socket_t client = NULL;
        infra_net_addr_t client_addr = {0};
        err = infra_net_accept(g_context.listen_sock, &client, &client_addr);
        if (err != INFRA_OK) {
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                continue;
            }
            break;
        }

        memkv_conn_t* conn = NULL;
        err = create_connection(client, &conn);
        if (err != INFRA_OK) {
            infra_net_close(client);
            continue;
        }

        err = infra_thread_pool_submit(g_context.pool, handle_connection, conn);
        if (err != INFRA_OK) {
            destroy_connection(conn);
            continue;
        }
    }

    return INFRA_OK;
}

infra_error_t memkv_stop(void) {
    if (!g_context.is_running) {
        return INFRA_ERROR_NOT_FOUND;
    }

    g_context.is_running = false;

    if (g_context.accept_thread) {
        infra_thread_join(g_context.accept_thread);
        g_context.accept_thread = NULL;
    }

    if (g_context.listen_sock) {
        infra_net_close(g_context.listen_sock);
        g_context.listen_sock = NULL;
    }

    return INFRA_OK;
}

bool memkv_is_running(void) {
    return g_context.is_running;
}

//-----------------------------------------------------------------------------
// Connection Management
//-----------------------------------------------------------------------------

static infra_error_t create_connection(infra_socket_t sock, memkv_conn_t** conn) {
    memkv_conn_t* new_conn = malloc(sizeof(memkv_conn_t));
    if (!new_conn) {
        return MEMKV_ERROR_NO_MEMORY;
    }

    memset(new_conn, 0, sizeof(memkv_conn_t));
    new_conn->sock = sock;
    new_conn->is_active = true;

    // Allocate buffer
    new_conn->buffer = malloc(MEMKV_BUFFER_SIZE);
    if (!new_conn->buffer) {
        free(new_conn);
        return MEMKV_ERROR_NO_MEMORY;
    }

    // Set socket options
    infra_error_t err;
    
    err = infra_net_set_nonblock(sock, true);
    if (err != INFRA_OK) goto error;

    err = infra_net_set_timeout(sock, 5000);
    if (err != INFRA_OK) goto error;

    err = infra_net_set_nodelay(sock, true);
    if (err != INFRA_OK) goto error;

    err = infra_net_set_keepalive(sock, true);
    if (err != INFRA_OK) goto error;

    *conn = new_conn;
    return INFRA_OK;

error:
    destroy_connection(new_conn);
    return err;
}

static void destroy_connection(memkv_conn_t* conn) {
    if (!conn) return;
    
    conn->is_active = false;
    
    if (conn->buffer) {
        free(conn->buffer);
        conn->buffer = NULL;
    }
    
    if (conn->sock) {
        infra_net_close(conn->sock);
        conn->sock = NULL;
    }
    
    free(conn);
}

static void* handle_connection(void* arg) {
    memkv_conn_t* conn = (memkv_conn_t*)arg;
    if (!conn) return NULL;

    while (conn->is_active) {
        size_t bytes_read = 0;
        infra_error_t err = infra_net_recv(conn->sock, 
                                         conn->buffer + conn->buffer_used,
                                         MEMKV_BUFFER_SIZE - conn->buffer_used,
                                         &bytes_read);
        
        if (err == INFRA_ERROR_TIMEOUT) {
            continue;
        } else if (err != INFRA_OK || bytes_read == 0) {
            break;
        }

        conn->buffer_used += bytes_read;

        err = memkv_cmd_process(conn);
        if (err != INFRA_OK && err != INFRA_ERROR_WOULD_BLOCK) {
            break;
        }
    }

    destroy_connection(conn);
    return NULL;
}

//-----------------------------------------------------------------------------
// Command Handler
//-----------------------------------------------------------------------------

infra_error_t memkv_cmd_handler(int argc, char** argv) {
    if (argc < 2) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    const char* port_str = NULL;
    bool start = false;
    bool stop = false;
    bool status = false;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--port=", 7) == 0) {
            port_str = argv[i] + 7;
        } else if (strcmp(argv[i], "--start") == 0) {
            start = true;
        } else if (strcmp(argv[i], "--stop") == 0) {
            stop = true;
        } else if (strcmp(argv[i], "--status") == 0) {
            status = true;
        }
    }

    if (status) {
        infra_printf("MemKV service is %s\n", 
            memkv_is_running() ? "running" : "stopped");
        return INFRA_OK;
    }

    if (stop) {
        return memkv_stop();
    }

    if (start) {
        uint16_t port = MEMKV_DEFAULT_PORT;
        if (port_str) {
            char* endptr;
            long p = strtol(port_str, &endptr, 10);
            if (*endptr != '\0' || p <= 0 || p > 65535) {
                return INFRA_ERROR_INVALID_PARAM;
            }
            port = (uint16_t)p;
        }

        infra_config_t config = INFRA_DEFAULT_CONFIG;
        infra_error_t err = memkv_init(port, &config);
        if (err != INFRA_OK) {
            return err;
        }

        err = memkv_start();
        if (err != INFRA_OK) {
            memkv_cleanup();
            return err;
        }

        return INFRA_OK;
    }

    return INFRA_ERROR_INVALID_OPERATION;
}
