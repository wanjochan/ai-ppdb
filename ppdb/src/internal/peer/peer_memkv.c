#include "internal/peer/peer_memkv.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_platform.h"
#include "internal/poly/poly_hashtable.h"
#include "internal/poly/poly_atomic.h"
#include "internal/peer/peer_service.h"

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// 命令类型
typedef enum memkv_cmd_type {
    CMD_UNKNOWN = 0,
    CMD_SET,
    CMD_ADD,
    CMD_REPLACE,
    CMD_APPEND,
    CMD_PREPEND,
    CMD_CAS,
    CMD_GET,
    CMD_GETS,
    CMD_DELETE,
    CMD_INCR,
    CMD_DECR,
    CMD_TOUCH,
    CMD_GAT,
    CMD_FLUSH,
    CMD_STATS,
    CMD_VERSION,
    CMD_QUIT
} memkv_cmd_type_t;

// 命令状态
typedef enum memkv_cmd_state {
    CMD_STATE_INIT = 0,
    CMD_STATE_READ_CMD,
    CMD_STATE_READ_DATA,
    CMD_STATE_EXECUTE,
    CMD_STATE_COMPLETE
} memkv_cmd_state_t;

// 命令结构
typedef struct memkv_cmd {
    memkv_cmd_type_t type;     // 命令类型
    memkv_cmd_state_t state;   // 命令状态
    char* key;                 // 键
    size_t key_size;          // 键大小
    void* data;               // 数据
    size_t data_size;         // 数据大小
    size_t bytes_to_read;     // 需要读取的字节数
    uint32_t flags;           // 标志位
    uint32_t exptime;         // 过期时间
    uint64_t cas;             // CAS值
    int noreply;              // 是否不需要回复
    char** tokens;            // 命令参数
    int token_count;          // 参数数量
} memkv_cmd_t;

// 连接结构
typedef struct memkv_conn {
    infra_socket_t sock;      // 客户端套接字
    bool is_active;           // 连接是否活跃
    char* rbuf;              // 读缓冲区
    size_t rsize;            // 读缓冲区大小
    size_t rpos;             // 读缓冲区位置
    char* wbuf;              // 写缓冲区
    size_t wsize;            // 写缓冲区大小
    size_t wpos;             // 写缓冲区位置
    memkv_cmd_t cmd;         // 当前命令
    time_t last_cmd_time;    // 最后命令时间
} memkv_conn_t;

// 命令处理器结构
typedef struct memkv_cmd_handler {
    const char* name;                    // 命令名称
    memkv_cmd_type_t type;              // 命令类型
    infra_error_t (*handler)(memkv_conn_t* conn);  // 处理函数
    int min_args;                        // 最小参数数量
    int max_args;                        // 最大参数数量
    bool has_value;                      // 是否有值
} memkv_cmd_handler_t;

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
// Forward Declarations
//-----------------------------------------------------------------------------

// Service Interface Functions
static infra_error_t memkv_init(const infra_config_t* config);
static infra_error_t memkv_cleanup(void);
static infra_error_t memkv_start(void);
static infra_error_t memkv_stop(void);
static bool memkv_is_running(void);
static infra_error_t memkv_cmd_handler(int argc, char** argv);

// Connection Management Functions
static infra_error_t create_listener(void);
static infra_error_t create_connection(infra_socket_t client_sock);
static void destroy_connection(memkv_conn_t* conn);
static void* handle_connection(void* arg);

// Command Processing Functions
static infra_error_t memkv_cmd_init(memkv_cmd_t* cmd);
static infra_error_t memkv_cmd_cleanup(memkv_cmd_t* cmd);
static infra_error_t memkv_cmd_process(memkv_conn_t* conn);
static infra_error_t parse_command(memkv_conn_t* conn);

// Item Management Functions
static bool item_is_expired(const memkv_item_t* item);
static infra_error_t item_alloc(const char* key, size_t key_size, const void* value, size_t value_size, uint32_t flags, uint32_t exptime, memkv_item_t** item);
static void item_free(memkv_item_t* item);

// Storage Operations
static infra_error_t store_with_lock(const char* key, size_t key_size, const void* value, size_t value_size, uint32_t flags, uint32_t exptime);
static infra_error_t get_with_lock(const char* key, size_t key_size, memkv_item_t** item);

// Statistics Functions
static void update_stats_cmd(memkv_cmd_type_t type);
static void update_stats_hit(void);
static void update_stats_miss(void);

// Communication Functions
static infra_error_t send_response(memkv_conn_t* conn, const char* response, size_t len);
static infra_error_t send_value_response(memkv_conn_t* conn, const memkv_item_t* item);

// Command Handlers
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

//-----------------------------------------------------------------------------
// Command Handlers Array
//-----------------------------------------------------------------------------

static const memkv_cmd_handler_t g_handlers[] = {
    {"set", CMD_SET, handle_set, 5, 6, true},
    {"add", CMD_ADD, handle_add, 5, 6, true},
    {"replace", CMD_REPLACE, handle_replace, 5, 6, true},
    {"append", CMD_APPEND, handle_append, 5, 6, true},
    {"prepend", CMD_PREPEND, handle_prepend, 5, 6, true},
    {"cas", CMD_CAS, handle_cas, 6, 7, true},
    {"get", CMD_GET, handle_get, 2, -1, false},
    {"gets", CMD_GETS, handle_gets, 2, -1, false},
    {"delete", CMD_DELETE, handle_delete, 2, 3, false},
    {"incr", CMD_INCR, handle_incr, 2, 3, false},
    {"decr", CMD_DECR, handle_decr, 2, 3, false},
    {"touch", CMD_TOUCH, handle_touch, 3, 4, false},
    {"gat", CMD_GAT, handle_gat, 3, -1, false},
    {"flush_all", CMD_FLUSH, handle_flush_all, 1, 3, false},
    {"stats", CMD_STATS, handle_stats, 1, 2, false},
    {"version", CMD_VERSION, handle_version, 1, 1, false},
    {"quit", CMD_QUIT, handle_quit, 1, 1, false},
};

static const size_t g_handler_count = sizeof(g_handlers) / sizeof(g_handlers[0]);

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

memkv_context_t g_memkv_context = {0};

peer_service_t g_memkv_service = {
    .config = {
        .name = "memkv",
        .type = SERVICE_TYPE_MEMKV,
        .options = memkv_options,
        .option_count = memkv_option_count,
        .config = NULL,
    },
    .state = SERVICE_STATE_STOPPED,
    .init = memkv_init,
    .cleanup = memkv_cleanup,
    .start = memkv_start,
    .stop = memkv_stop,
    .is_running = memkv_is_running,
    .cmd_handler = memkv_cmd_handler,
};

//-----------------------------------------------------------------------------
// Command Handlers
//-----------------------------------------------------------------------------

infra_error_t memkv_cmd_handler(int argc, char** argv) {
    if (argc < 2) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Parse command line options
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (strcmp(arg, "--port") == 0) {
            if (i + 1 >= argc) {
                return INFRA_ERROR_INVALID_PARAM;
            }
            g_memkv_context.port = atoi(argv[++i]);
        } else if (strcmp(arg, "--start") == 0) {
            return memkv_start();
        } else if (strcmp(arg, "--stop") == 0) {
            return memkv_stop();
        } else if (strcmp(arg, "--status") == 0) {
            printf("MemKV service is %s\n", g_memkv_context.is_running ? "running" : "stopped");
            return INFRA_OK;
        }
    }

    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Connection Management
//-----------------------------------------------------------------------------

static infra_error_t create_listener(void) {
    infra_error_t err;
    infra_socket_t sock;
    infra_config_t config = INFRA_DEFAULT_CONFIG;

    // Create socket
    err = infra_net_create(&sock, false, &config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create socket: %d", err);
        return err;
    }

    // Set socket options
    err = infra_net_set_reuseaddr(sock, true);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set socket option: %d", err);
        infra_net_close(sock);
        return err;
    }

    // Bind socket
    infra_net_addr_t addr = {
        .host = "127.0.0.1",
        .port = g_memkv_context.port
    };
    err = infra_net_bind(sock, &addr);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to bind socket: %d", err);
        infra_net_close(sock);
        return err;
    }

    // Listen
    err = infra_net_listen(sock);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to listen on socket: %d", err);
        infra_net_close(sock);
        return err;
    }

    g_memkv_context.sock = sock;
    INFRA_LOG_INFO("Listening on port %d", g_memkv_context.port);

    return INFRA_OK;
}

static infra_error_t create_connection(infra_socket_t client_sock) {
    infra_error_t err;
    memkv_conn_t* conn = (memkv_conn_t*)malloc(sizeof(memkv_conn_t));
    if (!conn) {
        INFRA_LOG_ERROR("Failed to allocate connection");
        return INFRA_ERROR_NO_MEMORY;
    }

    // Initialize connection
    bzero(conn, sizeof(memkv_conn_t));
    conn->sock = client_sock;
    conn->is_active = true;
    conn->last_cmd_time = time(NULL);

    // Allocate buffers
    conn->rbuf = (char*)malloc(MEMKV_BUFFER_SIZE);
    conn->wbuf = (char*)malloc(MEMKV_BUFFER_SIZE);
    if (!conn->rbuf || !conn->wbuf) {
        INFRA_LOG_ERROR("Failed to allocate buffers");
        if (conn->rbuf) free(conn->rbuf);
        if (conn->wbuf) free(conn->wbuf);
        free(conn);
        return INFRA_ERROR_NO_MEMORY;
    }

    conn->rsize = MEMKV_BUFFER_SIZE;
    conn->wsize = MEMKV_BUFFER_SIZE;
    conn->rpos = 0;
    conn->wpos = 0;

    // Initialize command
    memkv_cmd_init(&conn->cmd);

    // Create thread
    infra_thread_t thread;
    err = infra_thread_create(&thread, handle_connection, conn);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create thread: %d", err);
        destroy_connection(conn);
        return err;
    }

    return INFRA_OK;
}

static void destroy_connection(memkv_conn_t* conn) {
    if (!conn) return;

    // Close socket
    if (conn->sock != 0) {
        infra_net_close(conn->sock);
        conn->sock = 0;
    }

    // Free buffers
    if (conn->rbuf) {
        free(conn->rbuf);
        conn->rbuf = NULL;
    }
    if (conn->wbuf) {
        free(conn->wbuf);
        conn->wbuf = NULL;
    }

    // Cleanup command
    memkv_cmd_cleanup(&conn->cmd);

    // Free connection
    free(conn);
}

static void* handle_connection(void* arg) {
    memkv_conn_t* conn = (memkv_conn_t*)arg;
    if (!conn) return NULL;

    while (conn->is_active) {
        // Process command
        infra_error_t err = memkv_cmd_process(conn);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to process command: %d", err);
            break;
        }

        // Check idle timeout
        time_t now = time(NULL);
        if (now - conn->last_cmd_time > MEMKV_IDLE_TIMEOUT) {
            INFRA_LOG_DEBUG("Connection idle timeout");
            break;
        }
    }

    // Cleanup connection
    destroy_connection(conn);
    return NULL;
}

//-----------------------------------------------------------------------------
// Command Handler Implementation
//-----------------------------------------------------------------------------

static infra_error_t handle_set(memkv_conn_t* conn) {
    if (!conn || conn->cmd.token_count < 5) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Parse command parameters
    const char* key = conn->cmd.tokens[1];
    uint32_t flags = (uint32_t)strtoul(conn->cmd.tokens[2], NULL, 10);
    uint32_t exptime = (uint32_t)strtoul(conn->cmd.tokens[3], NULL, 10);
    size_t bytes = (size_t)strtoul(conn->cmd.tokens[4], NULL, 10);
    bool noreply = (conn->cmd.token_count > 5 && strcmp(conn->cmd.tokens[5], "noreply") == 0);

    // Validate parameters
    if (strlen(key) > MEMKV_MAX_KEY_SIZE) {
        send_response(conn, "CLIENT_ERROR key is too long\r\n", 28);
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (bytes > MEMKV_MAX_VALUE_SIZE) {
        send_response(conn, "CLIENT_ERROR value is too large\r\n", 31);
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Read value data
    char* value = malloc(bytes + 2);  // +2 for \r\n
    if (!value) {
        send_response(conn, "SERVER_ERROR out of memory\r\n", 27);
        return INFRA_ERROR_NO_MEMORY;
    }

    size_t bytes_read = 0;
    while (bytes_read < bytes + 2) {
        size_t remaining = bytes + 2 - bytes_read;
        size_t received = 0;
        infra_error_t err = infra_net_recv(conn->sock, value + bytes_read, remaining, &received);
        if (err != INFRA_OK) {
            free(value);
            return err;
        }
        if (received == 0) {
            free(value);
            return INFRA_ERROR_CLOSED;
        }
        bytes_read += received;
    }

    // Verify \r\n
    if (value[bytes] != '\r' || value[bytes + 1] != '\n') {
        free(value);
        send_response(conn, "CLIENT_ERROR bad data chunk\r\n", 28);
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Create item
    memkv_item_t* item = create_item(key, value, bytes, flags, exptime);
    free(value);

    if (!item) {
        send_response(conn, "SERVER_ERROR out of memory\r\n", 27);
        return INFRA_ERROR_NO_MEMORY;
    }

    // Store item
    infra_mutex_lock(&g_memkv_context.mutex);
    
    // Update stats before storing
    poly_atomic_inc(&g_memkv_context.stats.curr_items);
    poly_atomic_inc(&g_memkv_context.stats.total_items);
    poly_atomic_add(&g_memkv_context.stats.bytes, bytes);

    // Store the item
    infra_error_t err = poly_hashtable_put(g_memkv_context.store, key, strlen(key), item);
    
    infra_mutex_unlock(&g_memkv_context.mutex);

    if (err != INFRA_OK) {
        destroy_item(item);
        send_response(conn, "SERVER_ERROR storage error\r\n", 26);
        return err;
    }

    // Send response
    if (!noreply) {
        send_response(conn, "STORED\r\n", 8);
    }

    return INFRA_OK;
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
    if (!conn || conn->cmd.token_count < 2) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Lock the store
    infra_mutex_lock(&g_memkv_context.mutex);

    // Process each key
    bool found_any = false;
    for (int i = 1; i < conn->cmd.token_count; i++) {
        const char* key = conn->cmd.tokens[i];
        
        // Get item from store
        void* value = NULL;
        infra_error_t err = poly_hashtable_get(g_memkv_context.store, key, strlen(key), &value);
        
        if (err == INFRA_OK && value) {
            memkv_item_t* item = (memkv_item_t*)value;
            
            // Check if item is expired
            if (item_is_expired(item)) {
                // Remove expired item
                poly_hashtable_remove(g_memkv_context.store, key, strlen(key));
                poly_atomic_dec(&g_memkv_context.stats.curr_items);
                poly_atomic_sub(&g_memkv_context.stats.bytes, item->value_size);
                destroy_item(item);
                update_stats_miss();
                continue;
            }

            // Send item data
            char header[256];
            int header_len = snprintf(header, sizeof(header),
                "VALUE %s %u %zu\r\n",
                item->key, item->flags, item->value_size);
            
            send_response(conn, header, header_len);
            send_response(conn, item->value, item->value_size);
            send_response(conn, "\r\n", 2);
            
            found_any = true;
            update_stats_hit();
        } else {
            update_stats_miss();
        }
    }

    infra_mutex_unlock(&g_memkv_context.mutex);

    // Send end marker
    send_response(conn, "END\r\n", 5);

    return INFRA_OK;
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
    // TODO: Implement version command
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_quit(memkv_conn_t* conn) {
    // TODO: Implement quit command
    return INFRA_ERROR_NOT_SUPPORTED;
}

//-----------------------------------------------------------------------------
// Service Management Implementation
//-----------------------------------------------------------------------------

static infra_error_t memkv_init(const infra_config_t* config) {
    if (g_memkv_context.is_running) {
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    // Initialize context
    bzero(&g_memkv_context, sizeof(g_memkv_context));
    g_memkv_context.port = 11211;  // Default memcached port
    g_memkv_context.sock = 0;

    // Create hash table for storage
    infra_error_t err = poly_hashtable_create(1024, hash_key, compare_key, destroy_item, &g_memkv_context.store);
    if (err != INFRA_OK) {
        return err;
    }

    // Initialize mutex
    err = infra_mutex_create(&g_memkv_context.mutex);
    if (err != INFRA_OK) {
        poly_hashtable_destroy(g_memkv_context.store);
        return err;
    }

    return INFRA_OK;
}

static infra_error_t memkv_cleanup(void) {
    if (g_memkv_context.is_running) {
        return INFRA_ERROR_BUSY;
    }

    // Cleanup mutex
    infra_mutex_destroy(&g_memkv_context.mutex);

    // Cleanup hash table
    if (g_memkv_context.store) {
        poly_hashtable_destroy(g_memkv_context.store);
        g_memkv_context.store = NULL;
    }

    return INFRA_OK;
}

static infra_error_t memkv_start(void) {
    infra_error_t err;

    if (g_memkv_context.is_running) {
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    // Create listener socket
    err = create_listener();
    if (err != INFRA_OK) {
        return err;
    }

    g_memkv_context.is_running = true;
    return INFRA_OK;
}

static infra_error_t memkv_stop(void) {
    if (!g_memkv_context.is_running) {
        return INFRA_ERROR_NOT_FOUND;
    }

    // Close listener socket
    if (g_memkv_context.sock != 0) {
        infra_net_close(g_memkv_context.sock);
        g_memkv_context.sock = 0;
    }

    g_memkv_context.is_running = false;
    return INFRA_OK;
}

bool memkv_is_running(void) {
    return g_memkv_context.is_running;
}

// Item management functions
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
    item->cas = g_memkv_context.cas_counter++;

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

bool item_is_expired(const memkv_item_t* item) {
    if (!item || item->exptime == 0) {
        return false;
    }

    time_t now = time(NULL);
    return now > item->exptime;
}

// Statistics functions
void update_stats_cmd(memkv_cmd_type_t type) {
    switch (type) {
        case CMD_SET:
        case CMD_ADD:
        case CMD_REPLACE:
        case CMD_APPEND:
        case CMD_PREPEND:
        case CMD_CAS:
            poly_atomic_inc(&g_memkv_context.stats.cmd_set);
            break;
        case CMD_GET:
        case CMD_GETS:
            poly_atomic_inc(&g_memkv_context.stats.cmd_get);
            break;
        case CMD_DELETE:
            poly_atomic_inc(&g_memkv_context.stats.cmd_delete);
            break;
        default:
            break;
    }
}

void update_stats_hit(void) {
    poly_atomic_inc(&g_memkv_context.stats.hits);
}

void update_stats_miss(void) {
    poly_atomic_inc(&g_memkv_context.stats.misses);
}

// Communication functions
infra_error_t send_response(memkv_conn_t* conn, const char* response, size_t len) {
    if (!conn || !response) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Check buffer size
    if (conn->wpos + len > conn->wsize) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // Copy response to write buffer
    memcpy(conn->wbuf + conn->wpos, response, len);
    conn->wpos += len;

    // Send response
    size_t sent = 0;
    while (sent < len) {
        size_t bytes_sent = 0;
        infra_error_t err = infra_net_send(conn->sock, conn->wbuf + sent, len - sent, &bytes_sent);
        if (err != INFRA_OK) {
            return err;
        }
        sent += bytes_sent;
    }

    // Reset write buffer
    conn->wpos = 0;

    return INFRA_OK;
}

infra_error_t send_value_response(memkv_conn_t* conn, const memkv_item_t* item) {
    if (!conn || !item) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Format response header
    char header[256];
    int header_len = snprintf(header, sizeof(header), 
        "VALUE %s %u %zu %llu\r\n", 
        item->key, item->flags, item->value_size, item->cas);

    // Send header
    infra_error_t err = send_response(conn, header, header_len);
    if (err != INFRA_OK) {
        return err;
    }

    // Send value
    err = send_response(conn, item->value, item->value_size);
    if (err != INFRA_OK) {
        return err;
    }

    // Send end marker
    return send_response(conn, "\r\n", 2);
}

// Command processing initialization and cleanup
infra_error_t memkv_cmd_init(memkv_cmd_t* cmd) {
    if (!cmd) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    bzero(cmd, sizeof(memkv_cmd_t));
    cmd->type = CMD_UNKNOWN;
    cmd->state = CMD_STATE_INIT;
    return INFRA_OK;
}

infra_error_t memkv_cmd_cleanup(memkv_cmd_t* cmd) {
    if (!cmd) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (cmd->key) {
        free(cmd->key);
        cmd->key = NULL;
    }

    if (cmd->data) {
        free(cmd->data);
        cmd->data = NULL;
    }

    if (cmd->tokens) {
        for (int i = 0; i < cmd->token_count; i++) {
            if (cmd->tokens[i]) {
                free(cmd->tokens[i]);
            }
        }
        free(cmd->tokens);
        cmd->tokens = NULL;
    }

    cmd->token_count = 0;
    cmd->type = CMD_UNKNOWN;
    cmd->state = CMD_STATE_INIT;

    return INFRA_OK;
}

// Command processing
infra_error_t memkv_cmd_process(memkv_conn_t* conn) {
    infra_error_t err;

    // Read command
    err = parse_command(conn);
    if (err == INFRA_ERROR_WOULD_BLOCK) {
        return INFRA_OK;
    }
    if (err != INFRA_OK) {
        return err;
    }

    // Find handler
    const memkv_cmd_handler_t* handler = NULL;
    for (size_t i = 0; i < g_handler_count; i++) {
        if (strcmp(conn->cmd.tokens[0], g_handlers[i].name) == 0) {
            handler = &g_handlers[i];
            break;
        }
    }

    if (!handler) {
        send_response(conn, "ERROR\r\n", 7);
        return INFRA_ERROR_NOT_FOUND;
    }

    // Check argument count
    if (handler->min_args > conn->cmd.token_count ||
        (handler->max_args != -1 && handler->max_args < conn->cmd.token_count)) {
        send_response(conn, "CLIENT_ERROR bad command line format\r\n", 37);
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Execute handler
    err = handler->handler(conn);
    if (err != INFRA_OK) {
        return err;
    }

    // Update stats
    update_stats_cmd(handler->type);

    return INFRA_OK;
}

static infra_error_t parse_command(memkv_conn_t* conn) {
    // Read line
    char* line = conn->rbuf + conn->rpos;
    char* end = strstr(line, "\r\n");
    if (!end) {
        return INFRA_ERROR_WOULD_BLOCK;
    }

    // Calculate line length
    size_t len = end - line;
    *end = '\0';

    // Split into tokens
    char* token = strtok(line, " ");
    if (!token) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Allocate token array
    char** tokens = NULL;
    int token_count = 0;
    int token_capacity = 8;

    tokens = (char**)malloc(token_capacity * sizeof(char*));
    if (!tokens) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // Parse tokens
    while (token) {
        // Resize token array if needed
        if (token_count >= token_capacity) {
            token_capacity *= 2;
            char** new_tokens = (char**)realloc(tokens, token_capacity * sizeof(char*));
            if (!new_tokens) {
                for (int i = 0; i < token_count; i++) {
                    free(tokens[i]);
                }
                free(tokens);
                return INFRA_ERROR_NO_MEMORY;
            }
            tokens = new_tokens;
        }

        // Copy token
        tokens[token_count] = strdup(token);
        if (!tokens[token_count]) {
            for (int i = 0; i < token_count; i++) {
                free(tokens[i]);
            }
            free(tokens);
            return INFRA_ERROR_NO_MEMORY;
        }

        token_count++;
        token = strtok(NULL, " ");
    }

    // Update command
    conn->cmd.tokens = tokens;
    conn->cmd.token_count = token_count;

    // Update read position
    conn->rpos += len + 2;  // Skip \r\n

    return INFRA_OK;
}
