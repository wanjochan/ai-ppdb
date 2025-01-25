#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_platform.h"
#include "internal/peer/peer_memkv.h"
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
// Forward Declarations
//-----------------------------------------------------------------------------

// Command handlers
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

// Connection management
static infra_error_t create_listener(void);
static infra_error_t create_connection(infra_socket_t sock, memkv_conn_t** conn);
static void destroy_connection(memkv_conn_t* conn);
static void* handle_connection(void* arg);

// Command processing
static infra_error_t memkv_cmd_init(void);
static infra_error_t memkv_cmd_cleanup(void);
static infra_error_t memkv_cmd_process(memkv_conn_t* conn);
static infra_error_t memkv_parse_command(memkv_conn_t* conn);

// Item management
static memkv_item_t* create_item(const char* key, const void* value, size_t value_size, uint32_t flags, uint32_t exptime);
static void destroy_item(memkv_item_t* item);
static bool is_item_expired(const memkv_item_t* item);

// Storage operations
static infra_error_t store_with_lock(const char* key, const void* value, size_t value_size, uint32_t flags, uint32_t exptime);
static infra_error_t get_with_lock(const char* key, memkv_item_t** item);
static infra_error_t delete_with_lock(const char* key);

// Statistics
static void update_stats_set(size_t bytes);
static void update_stats_delete(size_t bytes);
static void update_stats_get(bool hit);

// Communication
static infra_error_t send_response(memkv_conn_t* conn, const char* response, size_t len);
static infra_error_t send_value_response(memkv_conn_t* conn, const memkv_item_t* item);

//-----------------------------------------------------------------------------
// Command Types
//-----------------------------------------------------------------------------

// Command types
typedef enum {
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

//-----------------------------------------------------------------------------
// Command Handler Structure
//-----------------------------------------------------------------------------

// Command handler structure
typedef struct {
    const char* name;
    memkv_cmd_type_t type;
    infra_error_t (*handler)(memkv_conn_t* conn);
    int min_args;
    int max_args;
    bool has_value;
} memkv_cmd_handler_t;

//-----------------------------------------------------------------------------
// Command States
//-----------------------------------------------------------------------------

// Command states
typedef enum {
    CMD_STATE_INIT = 0,
    CMD_STATE_READING_DATA,
    CMD_STATE_COMPLETE
} memkv_cmd_state_t;

//-----------------------------------------------------------------------------
// Command Structure
//-----------------------------------------------------------------------------

// Command structure
typedef struct {
    memkv_cmd_type_t type;
    memkv_cmd_state_t state;
    char* key;
    void* data;
    size_t bytes;
    uint32_t flags;
    uint32_t exptime;
    uint64_t cas;
    bool noreply;
} memkv_cmd_t;

//-----------------------------------------------------------------------------
// Connection Structure
//-----------------------------------------------------------------------------

// Connection structure
struct memkv_conn {
    infra_socket_t sock;              // Socket
    bool is_active;                   // Connection active
    char* buffer;                     // Command buffer
    size_t buffer_used;              // Used buffer size
    size_t buffer_read;              // Read buffer size
    memkv_cmd_t current_cmd;         // Current command
    char response[MEMKV_BUFFER_SIZE]; // Response buffer
    size_t response_len;             // Response length
};

//-----------------------------------------------------------------------------
// Command Handlers
//-----------------------------------------------------------------------------

// Command handlers
static const memkv_cmd_handler_t g_handlers[] = {
    {"set",      CMD_SET,     handle_set,      2, 6, true},
    {"add",      CMD_ADD,     handle_add,      2, 6, true},
    {"replace",  CMD_REPLACE, handle_replace,  2, 6, true},
    {"append",   CMD_APPEND,  handle_append,   2, 6, true},
    {"prepend",  CMD_PREPEND, handle_prepend,  2, 6, true},
    {"cas",      CMD_CAS,     handle_cas,      3, 7, true},
    {"get",      CMD_GET,     handle_get,      1, -1, false},
    {"gets",     CMD_GETS,    handle_gets,     1, -1, false},
    {"delete",   CMD_DELETE,  handle_delete,   1, 3, false},
    {"incr",     CMD_INCR,    handle_incr,     1, 3, false},
    {"decr",     CMD_DECR,    handle_decr,     1, 3, false},
    {"touch",    CMD_TOUCH,   handle_touch,    2, 3, false},
    {"gat",      CMD_GAT,     handle_gat,      2, -1, false},
    {"flush_all",CMD_FLUSH,   handle_flush_all,0, 2, false},
    {"stats",    CMD_STATS,   handle_stats,    0, 1, false},
    {"version",  CMD_VERSION, handle_version,  0, 1, false},
    {"quit",     CMD_QUIT,    handle_quit,     0, 1, false},
    {NULL,       CMD_UNKNOWN, NULL,            0, 0, false}
};

//-----------------------------------------------------------------------------
// Service Implementation
//-----------------------------------------------------------------------------

// 服务实例
peer_service_t g_memkv_service = {
    .config = {
        .name = "memkv",
        .type = SERVICE_TYPE_MEMKV,
        .options = memkv_options,
        .option_count = sizeof(memkv_options) / sizeof(memkv_options[0]),
        .config = NULL
    },
    .state = SERVICE_STATE_STOPPED,
    .init = memkv_init,
    .cleanup = memkv_cleanup,
    .start = memkv_start,
    .stop = memkv_stop,
    .is_running = memkv_is_running,
    .cmd_handler = memkv_cmd_handler
};

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

memkv_context_t g_memkv_context = {0};

//-----------------------------------------------------------------------------
// Command Handlers
//-----------------------------------------------------------------------------

infra_error_t memkv_cmd_handler(int argc, char** argv) {
    if (argc < 1) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Initialize service
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    infra_error_t err = memkv_init(&config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize memkv service: %d", err);
        return err;
    }

    // Parse command line
    bool should_start = false;
    uint16_t port = MEMKV_DEFAULT_PORT;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--start") == 0) {
            should_start = true;
        } else if (strcmp(argv[i], "--stop") == 0) {
            return memkv_stop();
        } else if (strcmp(argv[i], "--status") == 0) {
            infra_printf("Service is %s\n", 
                memkv_is_running() ? "running" : "stopped");
            return INFRA_OK;
        } else if (strncmp(argv[i], "--port=", 7) == 0) {
            port = (uint16_t)atoi(argv[i] + 7);
        } else if (strcmp(argv[i], "--port") == 0) {
            if (++i >= argc) {
                INFRA_LOG_ERROR("Missing port number");
                return INFRA_ERROR_INVALID_PARAM;
            }
            port = (uint16_t)atoi(argv[i]);
        }
    }

    // Set port
    g_memkv_context.port = port;

    // Start service
    if (should_start) {
        INFRA_LOG_DEBUG("Starting memkv service on port %d", port);
        err = memkv_start();
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to start memkv service: %d", err);
            return err;
        }
        INFRA_LOG_INFO("Memkv service started successfully");
    }

    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Connection Management
//-----------------------------------------------------------------------------

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
        .port = g_memkv_context.port
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

    g_memkv_context.listen_sock = listener;
    return INFRA_OK;
}

static infra_error_t create_connection(infra_socket_t sock, memkv_conn_t** conn) {
    if (!conn) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    memkv_conn_t* new_conn = (memkv_conn_t*)malloc(sizeof(memkv_conn_t));
    if (!new_conn) {
        return INFRA_ERROR_NO_MEMORY;
    }

    memset(new_conn, 0, sizeof(memkv_conn_t));
    new_conn->sock = sock;
    new_conn->is_active = true;

    *conn = new_conn;
    return INFRA_OK;
}

static void destroy_connection(memkv_conn_t* conn) {
    if (!conn) {
        return;
    }

    if (conn->sock) {
        infra_net_close(conn->sock);
    }

    if (conn->buffer) {
        free(conn->buffer);
    }

    free(conn);
}

static void* handle_connection(void* arg) {
    memkv_conn_t* conn = (memkv_conn_t*)arg;
    if (!conn) {
        return NULL;
    }

    while (conn->is_active && g_memkv_context.is_running) {
        infra_error_t err = memkv_cmd_process(conn);
        if (err != INFRA_OK) {
            break;
        }
    }

    destroy_connection(conn);
    return NULL;
}

//-----------------------------------------------------------------------------
// Command Handler Implementation
//-----------------------------------------------------------------------------

static infra_error_t handle_set(memkv_conn_t* conn) {
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_add(memkv_conn_t* conn) {
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_replace(memkv_conn_t* conn) {
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_append(memkv_conn_t* conn) {
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_prepend(memkv_conn_t* conn) {
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_cas(memkv_conn_t* conn) {
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_get(memkv_conn_t* conn) {
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_gets(memkv_conn_t* conn) {
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_delete(memkv_conn_t* conn) {
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_incr(memkv_conn_t* conn) {
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_decr(memkv_conn_t* conn) {
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_touch(memkv_conn_t* conn) {
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_gat(memkv_conn_t* conn) {
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_flush_all(memkv_conn_t* conn) {
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_stats(memkv_conn_t* conn) {
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_version(memkv_conn_t* conn) {
    return INFRA_ERROR_NOT_SUPPORTED;
}

static infra_error_t handle_quit(memkv_conn_t* conn) {
    return INFRA_ERROR_NOT_SUPPORTED;
}

//-----------------------------------------------------------------------------
// Service Management Implementation
//-----------------------------------------------------------------------------

infra_error_t memkv_init(const infra_config_t* config) {
    if (g_memkv_service.state != SERVICE_STATE_STOPPED) {
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    // Initialize context
    memset(&g_memkv_context, 0, sizeof(g_memkv_context));
    g_memkv_context.port = MEMKV_DEFAULT_PORT;

    // Initialize storage
    infra_error_t err = memkv_cmd_init();
    if (err != INFRA_OK) {
        return err;
    }

    g_memkv_service.state = SERVICE_STATE_STOPPED;
    return INFRA_OK;
}

infra_error_t memkv_cleanup(void) {
    if (g_memkv_service.state != SERVICE_STATE_STOPPED) {
        return INFRA_ERROR_BUSY;
    }

    // Clean up resources
    memkv_cmd_cleanup();
    memset(&g_memkv_context, 0, sizeof(g_memkv_context));

    return INFRA_OK;
}

infra_error_t memkv_start(void) {
    if (g_memkv_service.state != SERVICE_STATE_STOPPED) {
        return INFRA_ERROR_BUSY;
    }

    g_memkv_service.state = SERVICE_STATE_STARTING;
    
    // Create listener
    infra_error_t err = create_listener();
    if (err != INFRA_OK) {
        g_memkv_service.state = SERVICE_STATE_STOPPED;
        return err;
    }

    g_memkv_context.is_running = true;
    g_memkv_service.state = SERVICE_STATE_RUNNING;
    return INFRA_OK;
}

infra_error_t memkv_stop(void) {
    if (g_memkv_service.state != SERVICE_STATE_RUNNING) {
        return INFRA_ERROR_NOT_SUPPORTED;
    }

    g_memkv_service.state = SERVICE_STATE_STOPPING;
    g_memkv_context.is_running = false;

    // Close listener
    if (g_memkv_context.listen_sock) {
        infra_net_close(g_memkv_context.listen_sock);
        g_memkv_context.listen_sock = NULL;
    }

    g_memkv_service.state = SERVICE_STATE_STOPPED;
    return INFRA_OK;
}

bool memkv_is_running(void) {
    return g_memkv_service.state == SERVICE_STATE_RUNNING;
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
    item->cas = g_memkv_context.next_cas++;

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

// Statistics functions
void update_stats_set(size_t bytes) {
    poly_atomic_inc((poly_atomic_t*)&g_memkv_context.stats.cmd_set);
    poly_atomic_inc((poly_atomic_t*)&g_memkv_context.stats.total_items);
    poly_atomic_inc((poly_atomic_t*)&g_memkv_context.stats.curr_items);
    poly_atomic_add((poly_atomic_t*)&g_memkv_context.stats.bytes, bytes);
}

void update_stats_delete(size_t bytes) {
    poly_atomic_inc((poly_atomic_t*)&g_memkv_context.stats.cmd_delete);
    poly_atomic_dec((poly_atomic_t*)&g_memkv_context.stats.curr_items);
    poly_atomic_sub((poly_atomic_t*)&g_memkv_context.stats.bytes, bytes);
}

void update_stats_get(bool hit) {
    poly_atomic_inc((poly_atomic_t*)&g_memkv_context.stats.cmd_get);
    if (hit) {
        poly_atomic_inc((poly_atomic_t*)&g_memkv_context.stats.hits);
    } else {
        poly_atomic_inc((poly_atomic_t*)&g_memkv_context.stats.misses);
    }
}

// Communication functions
infra_error_t send_response(memkv_conn_t* conn, const char* response, size_t len) {
    return INFRA_ERROR_NOT_SUPPORTED;
}

// Command processing initialization and cleanup
infra_error_t memkv_cmd_init(void) {
    return INFRA_OK;
}

infra_error_t memkv_cmd_cleanup(void) {
    return INFRA_OK;
}

// Command processing
infra_error_t memkv_cmd_process(memkv_conn_t* conn) {
    if (!conn) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    infra_error_t err = memkv_parse_command(conn);
    if (err != INFRA_OK) {
        return err;
    }

    // Find command handler
    const memkv_cmd_handler_t* handler = NULL;
    for (size_t i = 0; g_handlers[i].name != NULL; i++) {
        if (strcmp(g_handlers[i].name, conn->buffer) == 0) {
            handler = &g_handlers[i];
            break;
        }
    }

    if (!handler) {
        return INFRA_ERROR_NOT_FOUND;
    }

    // Execute command
    return handler->handler(conn);
}

static infra_error_t memkv_parse_command(memkv_conn_t* conn) {
    return INFRA_ERROR_NOT_SUPPORTED;
}
