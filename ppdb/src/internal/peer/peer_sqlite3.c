#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_sync.h"
#include "internal/poly/poly_db.h"
#include "internal/poly/poly_poll.h"
#include "internal/peer/peer_service.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define SQLITE3_MAX_PATH_LEN 256
#define SQLITE3_MAX_SQL_LEN 4096
#define SQLITE3_MAX_CONNECTIONS 128

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// Connection state
typedef struct {
    infra_socket_t client;              // Client socket
    poly_db_t* db;                      // Database connection
    char buffer[SQLITE3_MAX_SQL_LEN];   // SQL buffer
} sqlite3_conn_t;

// Service state
typedef struct {
    char db_path[SQLITE3_MAX_PATH_LEN]; // Database path
    infra_socket_t listener;            // Listener socket
    volatile bool running;              // Service running flag
    infra_mutex_t mutex;                // Service mutex
    poly_poll_context_t* poll_ctx;      // Poll context
} sqlite3_service_t;

static sqlite3_service_t g_service = {0};

// Forward declarations
static infra_error_t sqlite3_init(const infra_config_t* config);
static infra_error_t sqlite3_cleanup(void);
static infra_error_t sqlite3_start(void);
static infra_error_t sqlite3_stop(void);
static bool sqlite3_is_running(void);
static infra_error_t sqlite3_cmd_handler(int argc, char** argv);

//-----------------------------------------------------------------------------
// Command Line Options
//-----------------------------------------------------------------------------

static const poly_cmd_option_t g_options[] = {
    {
        .name = "db",
        .desc = "Database file path",
        .has_value = true
    },
    {
        .name = "port",
        .desc = "Listen port (default: 5433)",
        .has_value = true
    }
};

//-----------------------------------------------------------------------------
// Service Configuration
//-----------------------------------------------------------------------------

peer_service_t g_sqlite3_service = {
    .config = {
        .name = "sqlite3",
        .type = SERVICE_TYPE_SQLITE3,
        .options = g_options,
        .option_count = sizeof(g_options) / sizeof(g_options[0]),
        .config = NULL
    },
    .state = SERVICE_STATE_STOPPED,
    .init = sqlite3_init,
    .cleanup = sqlite3_cleanup,
    .start = sqlite3_start,
    .stop = sqlite3_stop,
    .is_running = sqlite3_is_running,
    .cmd_handler = sqlite3_cmd_handler
};

//-----------------------------------------------------------------------------
// Connection handling
//-----------------------------------------------------------------------------

static sqlite3_conn_t* sqlite3_conn_create(infra_socket_t client) {
    sqlite3_conn_t* conn = (sqlite3_conn_t*)infra_malloc(sizeof(sqlite3_conn_t));
    if (!conn) {
        return NULL;
    }
    
    conn->client = client;
    conn->db = NULL;
    
    // Open database connection
    poly_db_config_t config = {
        .type = POLY_DB_TYPE_SQLITE,
        .url = g_service.db_path,
        .max_memory = 0,
        .read_only = false,
        .plugin_path = NULL,
        .allow_fallback = false
    };
    
    infra_error_t err = poly_db_open(&config, &conn->db);
    if (err != INFRA_OK) {
        infra_free(conn);
        return NULL;
    }
    
    return conn;
}

static void sqlite3_conn_destroy(sqlite3_conn_t* conn) {
    if (conn) {
        if (conn->client) {
            infra_net_close(conn->client);
        }
        if (conn->db) {
            poly_db_close(conn->db);
        }
        infra_free(conn);
    }
}

//-----------------------------------------------------------------------------
// Request handling
//-----------------------------------------------------------------------------

static void handle_request_wrapper(void* args) {
    poly_poll_handler_args_t* handler_args = (poly_poll_handler_args_t*)args;
    char client_addr[POLY_MAX_ADDR_LEN];
    infra_net_addr_t addr = {0};
    infra_net_getsockname(handler_args->client, &addr);
    infra_net_addr_to_str(&addr, client_addr, sizeof(client_addr));

    // Create connection state
    sqlite3_conn_t* conn = sqlite3_conn_create(handler_args->client);
    if (!conn) {
        const char* error_msg = "Failed to create connection\n";
        size_t sent;
        infra_net_send(handler_args->client, error_msg, strlen(error_msg), &sent);
        infra_net_close(handler_args->client);
        return;
    }

    // Process requests
    while (g_service.running) {
        size_t received = 0;
        infra_error_t err = infra_net_recv(conn->client, conn->buffer, sizeof(conn->buffer), &received);
        if (err != INFRA_OK || received == 0) {
            break;
        }

        // Execute SQL and send response
        err = poly_db_exec(conn->db, conn->buffer);
        if (err != INFRA_OK) {
            const char* error_msg = "SQL execution failed\n";
            size_t sent;
            infra_net_send(conn->client, error_msg, strlen(error_msg), &sent);
        }
    }

    sqlite3_conn_destroy(conn);
}

//-----------------------------------------------------------------------------
// Service interface
//-----------------------------------------------------------------------------

static infra_error_t sqlite3_init(const infra_config_t* config) {
    infra_error_t err = infra_mutex_create(&g_service.mutex);
    if (err != INFRA_OK) {
        return err;
    }
    return INFRA_OK;
}

static infra_error_t sqlite3_start(void) {
    // Initialize poll context
    poly_poll_config_t poll_config = {
        .min_threads = 1,
        .max_threads = 4,
        .queue_size = 1000,
        .max_listeners = 1
    };

    g_service.poll_ctx = (poly_poll_context_t*)infra_malloc(sizeof(poly_poll_context_t));
    if (!g_service.poll_ctx) {
        return INFRA_ERROR_NO_MEMORY;
    }

    infra_error_t err = poly_poll_init(g_service.poll_ctx, &poll_config);
    if (err != INFRA_OK) {
        infra_free(g_service.poll_ctx);
        g_service.poll_ctx = NULL;
        return err;
    }

    // Create listener socket
    infra_net_addr_t addr = {
        .host = NULL,
        .port = 5433
    };

    infra_socket_t listener;
    err = infra_net_create(&listener, false, NULL);
    if (err != INFRA_OK) {
        poly_poll_cleanup(g_service.poll_ctx);
        infra_free(g_service.poll_ctx);
        g_service.poll_ctx = NULL;
        return err;
    }
    g_service.listener = listener;

    err = infra_net_bind(g_service.listener, &addr);
    if (err != INFRA_OK) {
        infra_net_close(g_service.listener);
        poly_poll_cleanup(g_service.poll_ctx);
        infra_free(g_service.poll_ctx);
        g_service.poll_ctx = NULL;
        return INFRA_ERROR_OPEN_FAILED;
    }

    err = infra_net_listen(g_service.listener);
    if (err != INFRA_OK) {
        infra_net_close(g_service.listener);
        poly_poll_cleanup(g_service.poll_ctx);
        infra_free(g_service.poll_ctx);
        g_service.poll_ctx = NULL;
        return err;
    }

    // Add listener to poll context
    poly_poll_listener_t listener_config = {
        .bind_port = 5433,
        .user_data = NULL
    };
    strcpy(listener_config.bind_addr, "0.0.0.0");

    err = poly_poll_add_listener(g_service.poll_ctx, &listener_config);
    if (err != INFRA_OK) {
        infra_net_close(g_service.listener);
        poly_poll_cleanup(g_service.poll_ctx);
        infra_free(g_service.poll_ctx);
        g_service.poll_ctx = NULL;
        return err;
    }

    // Set connection handler
    poly_poll_set_handler(g_service.poll_ctx, handle_request_wrapper);

    // Start polling
    g_service.running = true;
    return poly_poll_start(g_service.poll_ctx);
}

static infra_error_t sqlite3_stop(void) {
    if (!g_service.running) {
        return INFRA_OK;
    }
    
    g_service.running = false;
    
    if (g_service.listener) {
        infra_net_close(g_service.listener);
        g_service.listener = NULL;
    }
    
    if (g_service.poll_ctx) {
        poly_poll_stop(g_service.poll_ctx);
        poly_poll_cleanup(g_service.poll_ctx);
        infra_free(g_service.poll_ctx);
        g_service.poll_ctx = NULL;
    }
    
    return INFRA_OK;
}

static infra_error_t sqlite3_cleanup(void) {
    infra_mutex_destroy(g_service.mutex);
    return INFRA_OK;
}

static bool sqlite3_is_running(void) {
    return g_service.running;
}

//-----------------------------------------------------------------------------
// Command handler
//-----------------------------------------------------------------------------

static infra_error_t sqlite3_cmd_handler(int argc, char** argv) {
    bool start = false;
    bool stop = false;
    bool status = false;
    const char* db_path = NULL;
    int port = 5433;

    // Parse command line options
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-' || argv[i][1] != '-') {
            continue;
        }

        const char* option = argv[i] + 2;
        if (strcmp(option, "start") == 0) {
            start = true;
        } else if (strcmp(option, "stop") == 0) {
            stop = true;
        } else if (strcmp(option, "status") == 0) {
            status = true;
        } else {
            const char* value = strchr(option, '=');
            if (!value) {
                continue;
            }
            value++;  // Skip '='

            size_t name_len = value - option - 1;
            if (strncmp(option, "db", name_len) == 0) {
                db_path = value;
            } else if (strncmp(option, "port", name_len) == 0) {
                port = atoi(value);
                if (port <= 0 || port > 65535) {
                    INFRA_LOG_ERROR("Invalid port number: %d", port);
                    return INFRA_ERROR_INVALID_PARAM;
                }
            }
        }
    }

    // Check for conflicting options
    if ((start && stop) || (start && status) || (stop && status)) {
        INFRA_LOG_ERROR("Only one of --start, --stop, or --status can be specified");
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Handle commands
    if (start) {
        if (!db_path) {
            INFRA_LOG_ERROR("Database path not specified (use --db=<path>)");
            return INFRA_ERROR_INVALID_PARAM;
        }
        strncpy(g_service.db_path, db_path, sizeof(g_service.db_path) - 1);
        return sqlite3_start();
    } else if (stop) {
        return sqlite3_stop();
    } else if (status) {
        infra_printf("SQLite3 service is %s\n", 
            sqlite3_is_running() ? "running" : "stopped");
        return INFRA_OK;
    }

    INFRA_LOG_ERROR("No action specified (use --start, --stop, or --status)");
    return INFRA_ERROR_INVALID_PARAM;
}
