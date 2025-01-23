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

static memkv_context_t g_context;

//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------

static infra_error_t create_listener(void);
static infra_error_t process_command(memkv_conn_t* conn);
static infra_error_t parse_command(memkv_conn_t* conn);
static infra_error_t execute_command(memkv_conn_t* conn);
static infra_error_t send_response(memkv_conn_t* conn, const char* response);

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

// Create connection
static infra_error_t create_connection(infra_socket_t sock, memkv_conn_t** conn) {
    memkv_conn_t* new_conn = malloc(sizeof(memkv_conn_t));
    if (!new_conn) {
        return MEMKV_ERROR_NO_MEMORY;
    }

    memset(new_conn, 0, sizeof(memkv_conn_t));
    new_conn->sock = sock;
    new_conn->is_active = true;
    new_conn->buffer_used = 0;
    new_conn->buffer_read = 0;
    new_conn->cmd.state = CMD_STATE_INIT;

    infra_atomic_inc(&g_context.stats.curr_conns);
    *conn = new_conn;
    return MEMKV_OK;
}

// Destroy connection
static void destroy_connection(memkv_conn_t* conn) {
    if (!conn) return;
    
    if (conn->cmd.key) {
        free(conn->cmd.key);
    }
    if (conn->cmd.data) {
        free(conn->cmd.data);
    }
    
    free(conn->buffer);
    free(conn);
}

// Create item
memkv_item_t* create_item(const char* key, const void* value, size_t value_size, uint32_t flags, uint32_t exptime) {
    memkv_item_t* item = (memkv_item_t*)malloc(sizeof(memkv_item_t));
    if (!item) {
        return NULL;
    }

    item->key = strdup(key);
    if (!item->key) {
        free(item);
        return NULL;
    }

    item->data = malloc(value_size);
    if (!item->data) {
        free(item->key);
        free(item);
        return NULL;
    }

    memcpy(item->data, value, value_size);
    item->bytes = value_size;
    item->flags = flags;
    item->exptime = exptime;
    item->cas = 0; // TODO: 生成唯一的 CAS 值
    item->ctime = time(NULL);
    item->atime = item->ctime;

    return item;
}

// Destroy item
void destroy_item(memkv_item_t* item) {
    if (item) {
        free(item->key);
        free(item->data);
        free(item);
    }
}

// Check if item is expired
bool is_item_expired(const memkv_item_t* item) {
    if (item->exptime == 0) {
        return false;
    }
    return time(NULL) > item->exptime;
}

// Update statistics
static void update_stats_set(size_t value_size) {
    infra_atomic_inc(&g_context.stats.cmd_set);
    infra_atomic_inc(&g_context.stats.curr_items);
    infra_atomic_inc(&g_context.stats.total_items);
    infra_atomic_add(&g_context.stats.curr_bytes, value_size);
}

static void update_stats_get(bool hit) {
    infra_atomic_inc(&g_context.stats.cmd_get);
    if (hit) {
        infra_atomic_inc(&g_context.stats.hits);
    } else {
        infra_atomic_inc(&g_context.stats.misses);
    }
}

static void update_stats_delete(size_t value_size) {
    infra_atomic_inc(&g_context.stats.cmd_delete);
    infra_atomic_dec(&g_context.stats.curr_items);
    infra_atomic_sub(&g_context.stats.curr_bytes, value_size);
}

//-----------------------------------------------------------------------------
// Functions
//-----------------------------------------------------------------------------

// Hash function
static uint64_t hash_fn(const void* key) {
    const char* str = (const char*)key;
    uint64_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// Compare function
static bool compare_fn(const void* key1, const void* key2) {
    return strcmp((const char*)key1, (const char*)key2) == 0;
}

// Initialize MemKV
infra_error_t memkv_init(uint16_t port) {
    // Initialize global context
    memset(&g_context, 0, sizeof(g_context));
    g_context.port = port;

    // Create storage hash table
    infra_error_t err = poly_hashtable_create(1024, hash_fn, compare_fn, &g_context.store);
    if (err != INFRA_OK) {
        return err;
    }

    // Initialize mutex
    err = infra_mutex_create(&g_context.store_mutex);
    if (err != INFRA_OK) {
        poly_hashtable_destroy(g_context.store);
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
        infra_mutex_destroy(&g_context.store_mutex);
        poly_hashtable_destroy(g_context.store);
        return err;
    }

    return INFRA_OK;
}

infra_error_t memkv_cleanup(void) {
    if (g_context.running) {
        return INFRA_ERROR_BUSY;
    }

    if (g_context.store) {
        infra_mutex_lock(&g_context.store_mutex);
        poly_hashtable_foreach(g_context.store, 
            (poly_hashtable_iter_fn)destroy_item, NULL);
        poly_hashtable_destroy(g_context.store);
        g_context.store = NULL;
        infra_mutex_unlock(&g_context.store_mutex);
    }

    if (g_context.pool) {
        infra_thread_pool_destroy(g_context.pool);
        g_context.pool = NULL;
    }

    infra_net_close(g_context.listen_sock);
    g_context.listen_sock = NULL;
    infra_mutex_destroy(&g_context.store_mutex);

    return INFRA_OK;
}

static infra_error_t create_listener(void) {
    // Create listen socket
    infra_error_t err = infra_net_create(&g_context.listen_sock, false, NULL);
    if (err != INFRA_OK) {
        return err;
    }

    // Bind address
    infra_net_addr_t addr = {
        .host = NULL,  // Bind all addresses
        .port = g_context.port
    };
    err = infra_net_bind(g_context.listen_sock, &addr);
    if (err != INFRA_OK) {
        infra_net_close(g_context.listen_sock);
        g_context.listen_sock = NULL;
        return err;
    }

    // Start listening
    err = infra_net_listen(g_context.listen_sock);
    if (err != INFRA_OK) {
        infra_net_close(g_context.listen_sock);
        g_context.listen_sock = NULL;
        return err;
    }

    return INFRA_OK;
}

infra_error_t memkv_start(void) {
    if (g_context.running) {
        return INFRA_ERROR_BUSY;
    }

    // Create listen socket
    infra_error_t err = create_listener();
    if (err != INFRA_OK) {
        return err;
    }

    g_context.running = true;

    // Main loop: accept connections
    while (g_context.running) {
        infra_socket_t client_sock;
        infra_net_addr_t client_addr;
        err = infra_net_accept(g_context.listen_sock, &client_sock, &client_addr);
        
        if (!g_context.running) {
            break;
        }

        if (err != INFRA_OK) {
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                continue;
            }
            INFRA_LOG_ERROR("Accept failed: %d", err);
            continue;
        }

        // Create connection structure
        memkv_conn_t* conn;
        err = create_connection(client_sock, &conn);
        if (err != INFRA_OK) {
            infra_net_close(client_sock);
            continue;
        }

        // Submit to thread pool for processing
        err = infra_thread_pool_submit(g_context.pool, handle_connection, conn);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to submit task: %d", err);
            destroy_connection(conn);
            infra_net_close(client_sock);
        }
    }

    return INFRA_OK;
}

infra_error_t memkv_stop(void) {
    if (!g_context.running) {
        return INFRA_ERROR_NOT_READY;  // Service not running
    }

    g_context.running = false;
    infra_net_close(g_context.listen_sock);
    g_context.listen_sock = NULL;

    return INFRA_OK;
}

bool memkv_is_running(void) {
    return g_context.running;
}

//-----------------------------------------------------------------------------
// Connection Handling
//-----------------------------------------------------------------------------

static infra_error_t process_command(memkv_conn_t* conn) {
    while (conn->buffer_used > 0) {
        // 处理命令状态机
        switch (conn->cmd.state) {
            case CMD_STATE_INIT:
                // TODO: 解析命令头
                conn->cmd.state = CMD_STATE_READ_DATA;
                break;

            case CMD_STATE_READ_DATA:
                // 检查是否有足够的数据
                if (conn->buffer_used < conn->cmd.bytes) {
                    return INFRA_OK; // 需要更多数据
                }

                // 分配数据缓冲区
                conn->cmd.data = malloc(conn->cmd.bytes);
                if (!conn->cmd.data) {
                    send_response(conn, "SERVER_ERROR out of memory\r\n", 28);
                    return INFRA_ERROR_NO_MEMORY;
                }

                // 复制数据
                memcpy(conn->cmd.data, conn->buffer, 
                    conn->cmd.bytes);

                // 检查结束标记
                if (conn->buffer[conn->cmd.bytes] != '\r' ||
                    conn->buffer[conn->cmd.bytes + 1] != '\n') {
                    send_response(conn, "CLIENT_ERROR bad data chunk\r\n", 28);
                    return INFRA_ERROR_INVALID;
                }

                // 移动缓冲区
                memmove(conn->buffer, 
                    conn->buffer + conn->cmd.bytes + 2,
                    conn->buffer_used - conn->cmd.bytes - 2);
                conn->buffer_used -= conn->cmd.bytes + 2;
                conn->cmd.state = CMD_STATE_EXECUTING;
                break;

            case CMD_STATE_EXECUTING:
                // TODO: 执行命令
                // 清理命令
                if (conn->cmd.key) {
                    free(conn->cmd.key);
                    conn->cmd.key = NULL;
                }
                if (conn->cmd.data) {
                    free(conn->cmd.data);
                    conn->cmd.data = NULL;
                }
                
                conn->cmd.state = CMD_STATE_INIT;
                break;

            default:
                return INFRA_ERROR_INVALID;
        }
    }

    return INFRA_OK;
}

static infra_error_t send_response(memkv_conn_t* conn, const char* response, size_t len) {
    size_t sent;
    return infra_net_send(conn->sock, response, len, &sent);
}

static void* handle_connection(void* arg) {
    memkv_conn_t* conn = (memkv_conn_t*)arg;
    infra_error_t err;

    while (g_context.running) {
        // 读取命令
        size_t bytes_read;
        err = infra_net_recv(conn->sock,
                            conn->buffer + conn->buffer_used,
                            MEMKV_BUFFER_SIZE - conn->buffer_used,
                            &bytes_read);

        if (err != INFRA_OK || bytes_read == 0) {
            break;
        }

        conn->buffer_used += bytes_read;

        // 处理命令
        err = process_command(conn);
        if (err != INFRA_OK) {
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
        INFRA_LOG_ERROR("No command specified");
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Parse options
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

    // Process command
    if (status) {
        infra_printf("MemKV service is %s\n", 
            memkv_is_running() ? "running" : "stopped");
        return INFRA_OK;
    }

    if (stop) {
        return memkv_stop();
    }

    if (start) {
        if (!port_str) {
            INFRA_LOG_ERROR("Port not specified");
            return INFRA_ERROR_INVALID_PARAM;
        }

        // Parse port
        char* endptr;
        long port = strtol(port_str, &endptr, 10);
        if (*endptr != '\0' || port <= 0 || port > 65535) {
            INFRA_LOG_ERROR("Invalid port: %s", port_str);
            return INFRA_ERROR_INVALID_PARAM;
        }

        g_context.port = (uint16_t)port;
        return memkv_start();
    }

    INFRA_LOG_ERROR("Invalid command");
    return INFRA_ERROR_INVALID_OPERATION;  // Invalid command
}

const memkv_stats_t* memkv_get_stats(void) {
    return &g_context.stats;
}

static infra_error_t parse_command(memkv_conn_t* conn) {
    // TODO: Implement command parsing
    return INFRA_OK;
}

static infra_error_t execute_command(memkv_conn_t* conn) {
    // TODO: Implement command execution
    return INFRA_OK;
}
