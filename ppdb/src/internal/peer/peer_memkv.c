#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_platform.h"
#include "internal/peer/peer_memkv.h"
#include "internal/poly/poly_cmdline.h"

//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------

// Command processing functions
static infra_error_t handle_set(memkv_conn_t* conn);
static infra_error_t handle_add(memkv_conn_t* conn);
static infra_error_t handle_replace(memkv_conn_t* conn);
static infra_error_t handle_append(memkv_conn_t* conn);
static infra_error_t handle_prepend(memkv_conn_t* conn);
static infra_error_t handle_cas(memkv_conn_t* conn);
static infra_error_t handle_get(memkv_conn_t* conn);
static infra_error_t handle_gets(memkv_conn_t* conn);
static infra_error_t handle_delete(memkv_conn_t* conn);
static infra_error_t handle_incr(memkv_conn_t* conn);
static infra_error_t handle_decr(memkv_conn_t* conn);
static infra_error_t handle_touch(memkv_conn_t* conn);
static infra_error_t handle_gat(memkv_conn_t* conn);
static infra_error_t handle_flush_all(memkv_conn_t* conn);
static infra_error_t handle_stats(memkv_conn_t* conn);
static infra_error_t handle_version(memkv_conn_t* conn);
static infra_error_t handle_quit(memkv_conn_t* conn);

static infra_error_t create_listener(void);
static infra_error_t create_connection(infra_socket_t sock, memkv_conn_t** conn);
static void destroy_connection(memkv_conn_t* conn);
static void* handle_connection(void* arg);
static infra_error_t memkv_parse_command(memkv_conn_t* conn);

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

// 命令处理器表
static const memkv_cmd_handler_t g_handlers[] = {
    {"set",     CMD_SET,     handle_set,     5, 5, true},
    {"add",     CMD_ADD,     handle_add,     5, 5, true},
    {"replace", CMD_REPLACE, handle_replace, 5, 5, true},
    {"append",  CMD_APPEND,  handle_append,  5, 5, true},
    {"prepend", CMD_PREPEND, handle_prepend, 5, 5, true},
    {"cas",     CMD_CAS,     handle_cas,     6, 6, true},
    {"get",     CMD_GET,     handle_get,     2, -1, false},
    {"gets",    CMD_GETS,    handle_gets,    2, -1, false},
    {"incr",     CMD_INCR,     handle_incr,     3, 3, false},
    {"decr",     CMD_DECR,     handle_decr,     3, 3, false},
    {"touch",    CMD_TOUCH,    handle_touch,    3, 3, false},
    {"gat",      CMD_GAT,      handle_gat,      3, -1, false},
    {"flush_all",CMD_FLUSH,    handle_flush_all,1, 2, false},
    {"delete",  CMD_DELETE,  handle_delete,  2, 2, false},
    {"stats",   CMD_STATS,   handle_stats,   1, 2, false},
    {"version", CMD_VERSION, handle_version, 1, 1, false},
    {"quit",    CMD_QUIT,    handle_quit,    1, 1, false},
    {NULL,      CMD_UNKNOWN, NULL,          0, 0, false}
};

// 存储操作
static infra_error_t store_with_lock(const char* key, const void* value, size_t value_size, uint32_t flags, uint32_t exptime) {
    memkv_item_t* item = create_item(key, value, value_size, flags, exptime);
    if (!item) {
        return MEMKV_ERROR_NO_MEMORY;
    }

    infra_mutex_lock(&g_context.store_mutex);
    infra_error_t err = poly_hashtable_put(g_context.store, item->key, item);
    if (err == INFRA_OK) {
        update_stats_set(value_size);
    } else {
        destroy_item(item);
    }
    infra_mutex_unlock(&g_context.store_mutex);

    return err;
}

// 获取操作
static infra_error_t get_with_lock(const char* key, memkv_item_t** item) {
    infra_mutex_lock(&g_context.store_mutex);
    infra_error_t err = poly_hashtable_get(g_context.store, key, (void**)item);
    if (err == INFRA_OK && *item) {
        if (is_item_expired(*item)) {
            err = poly_hashtable_remove(g_context.store, key);
            if (err == INFRA_OK) {
                update_stats_delete((*item)->value_size);
                destroy_item(*item);
                *item = NULL;
            }
            err = MEMKV_ERROR_NOT_FOUND;
        }
    }
    infra_mutex_unlock(&g_context.store_mutex);
    return err;
}

// 删除操作
static infra_error_t delete_with_lock(const char* key) {
    memkv_item_t* item = NULL;
    infra_error_t err = get_with_lock(key, &item);
    if (err == INFRA_OK) {
        if (item) {
            err = poly_hashtable_remove(g_context.store, key);
            if (err == INFRA_OK) {
                update_stats_delete(item->value_size);
                destroy_item(item);
            }
        } else {
            err = MEMKV_ERROR_NOT_FOUND;
        }
    }
    return err;
}

// 发送值响应
static infra_error_t send_value_response(memkv_conn_t* conn, const memkv_item_t* item) {
    char header[256];
    size_t header_len = snprintf(header, sizeof(header), "VALUE %s %u %zu\r\n", 
        item->key, item->flags, item->value_size);
    infra_error_t err = send_response(conn, header, header_len);
    if (err != INFRA_OK) {
        return err;
    }

    err = send_response(conn, item->value, item->value_size);
    if (err != INFRA_OK) {
        return err;
    }

    return send_response(conn, "\r\n", 2);
}

//-----------------------------------------------------------------------------
// Command Handlers
//-----------------------------------------------------------------------------

static infra_error_t handle_set(memkv_conn_t* conn) {
    // TODO: Implement set command
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_add(memkv_conn_t* conn) {
    // TODO: Implement add command
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_replace(memkv_conn_t* conn) {
    // TODO: Implement replace command
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_append(memkv_conn_t* conn) {
    // TODO: Implement append command
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_prepend(memkv_conn_t* conn) {
    // TODO: Implement prepend command
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_cas(memkv_conn_t* conn) {
    // TODO: Implement cas command
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_get(memkv_conn_t* conn) {
    // TODO: Implement get command
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_gets(memkv_conn_t* conn) {
    // TODO: Implement gets command
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_delete(memkv_conn_t* conn) {
    // TODO: Implement delete command
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_incr(memkv_conn_t* conn) {
    // TODO: Implement incr command
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_decr(memkv_conn_t* conn) {
    // TODO: Implement decr command
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_touch(memkv_conn_t* conn) {
    // TODO: Implement touch command
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_gat(memkv_conn_t* conn) {
    // TODO: Implement gat command
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_flush_all(memkv_conn_t* conn) {
    // TODO: Implement flush_all command
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_stats(memkv_conn_t* conn) {
    // TODO: Implement stats command
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_version(memkv_conn_t* conn) {
    char response[64];
    int len = snprintf(response, sizeof(response), "VERSION %s\r\n", MEMKV_VERSION);
    return send_response(conn, response, len);
}

static infra_error_t handle_quit(memkv_conn_t* conn) {
    conn->is_active = false;
    return INFRA_OK;
}

static infra_error_t memkv_parse_command(memkv_conn_t* conn) {
    // TODO: Implement command parsing
    return INFRA_ERROR_NOT_SUPPORTED;
}

// 项目管理函数
memkv_item_t* create_item(const char* key, const void* value, size_t value_size, uint32_t flags, uint32_t exptime) {
    if (!key || !value || value_size == 0) {
        return NULL;
    }

    memkv_item_t* item = malloc(sizeof(memkv_item_t));
    if (!item) {
        return NULL;
    }

    item->key = strdup(key);
    if (!item->key) {
        free(item);
        return NULL;
    }

    item->value = malloc(value_size);
    if (!item->value) {
        free(item->key);
        free(item);
        return NULL;
    }

    memcpy(item->value, value, value_size);
    item->value_size = value_size;
    item->flags = flags;
    item->exptime = exptime ? time(NULL) + exptime : 0;
    item->cas = g_context.next_cas++;

    return item;
}

void destroy_item(memkv_item_t* item) {
    if (!item) {
        return;
    }
    if (item->key) {
        free(item->key);
    }
    if (item->value) {
        free(item->value);
    }
    free(item);
}

bool is_item_expired(const memkv_item_t* item) {
    if (!item || !item->exptime) {
        return false;
    }
    return time(NULL) > item->exptime;
}

// 统计函数
void update_stats_set(size_t bytes) {
    poly_atomic_inc((poly_atomic_t*)&g_context.stats.cmd_set);
    poly_atomic_inc((poly_atomic_t*)&g_context.stats.total_items);
    poly_atomic_inc((poly_atomic_t*)&g_context.stats.curr_items);
    poly_atomic_add((poly_atomic_t*)&g_context.stats.bytes, bytes);
}

void update_stats_delete(size_t bytes) {
    poly_atomic_inc((poly_atomic_t*)&g_context.stats.cmd_delete);
    poly_atomic_dec((poly_atomic_t*)&g_context.stats.curr_items);
    poly_atomic_sub((poly_atomic_t*)&g_context.stats.bytes, bytes);
}

void update_stats_get(bool hit) {
    poly_atomic_inc((poly_atomic_t*)&g_context.stats.cmd_get);
    if (hit) {
        poly_atomic_inc((poly_atomic_t*)&g_context.stats.hits);
    } else {
        poly_atomic_inc((poly_atomic_t*)&g_context.stats.misses);
    }
}

// 通信函数
infra_error_t send_response(memkv_conn_t* conn, const char* response, size_t len) {
    if (!conn || !response) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    size_t sent = 0;
    while (sent < len) {
        size_t bytes_sent = 0;
        infra_error_t err = infra_net_send(conn->sock, conn->response + sent, len - sent, &bytes_sent);
        if (err != INFRA_OK) {
            return err;
        }
        sent += bytes_sent;
    }
    return INFRA_OK;
}

// 命令处理初始化和清理
infra_error_t memkv_cmd_init(void) {
    infra_error_t err = poly_hashtable_create(1024, poly_hashtable_string_hash, 
        poly_hashtable_string_compare, &g_context.store);
    if (err != INFRA_OK) {
        return err;
    }

    err = infra_mutex_create(&g_context.store_mutex);
    if (err != INFRA_OK) {
        poly_hashtable_destroy(g_context.store);
        g_context.store = NULL;
        return err;
    }

    return INFRA_OK;
}

infra_error_t memkv_cmd_cleanup(void) {
    if (g_context.store) {
        infra_mutex_lock(&g_context.store_mutex);
        poly_hashtable_clear(g_context.store);
        poly_hashtable_destroy(g_context.store);
        g_context.store = NULL;
        infra_mutex_unlock(&g_context.store_mutex);
    }

    if (g_context.store_mutex) {
        infra_mutex_destroy(&g_context.store_mutex);
        g_context.store_mutex = NULL;
    }

    return INFRA_OK;
}

// 命令处理
infra_error_t memkv_cmd_process(memkv_conn_t* conn) {
    if (!conn) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Parse command
    infra_error_t err = memkv_parse_command(conn);
    if (err != INFRA_OK) {
        if (err == INFRA_ERROR_WOULD_BLOCK) {
            return err; // Need more data
        }
        send_response(conn, "ERROR\r\n", 7);
        return err;
    }

    // Find command handler
    const memkv_cmd_handler_t* handler = NULL;
    for (int i = 0; g_handlers[i].name != NULL; i++) {
        if (g_handlers[i].type == conn->current_cmd.type) {
            handler = &g_handlers[i];
            break;
        }
    }

    if (!handler) {
        send_response(conn, "ERROR\r\n", 7);
        return INFRA_ERROR_NOT_FOUND;
    }

    // Execute command
    err = handler->fn(conn);
    if (err != INFRA_OK) {
        if (err != INFRA_ERROR_WOULD_BLOCK) {
            send_response(conn, "ERROR\r\n", 7);
        }
        return err;
    }

    return INFRA_OK;
}
