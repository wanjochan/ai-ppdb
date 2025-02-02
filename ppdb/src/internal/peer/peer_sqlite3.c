#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_sync.h"
#include "internal/poly/poly_db.h"
#include "internal/poly/poly_poll.h"
#include "internal/peer/peer_service.h"
#include "internal/peer/peer_sqlite3.h"
// #include "internal/sqlite3/sqlite3.h"  // 修改 SQLite 头文件路径

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
    volatile bool is_closing;           // Connection closing flag
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
infra_error_t sqlite3_init(const infra_config_t* config);
infra_error_t sqlite3_cleanup(void);
infra_error_t sqlite3_start(void);
infra_error_t sqlite3_stop(void);
bool sqlite3_is_running(void);
infra_error_t sqlite3_cmd_handler(int argc, char** argv);

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
    .state = SERVICE_STATE_UNKNOWN,
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
        INFRA_LOG_ERROR("Failed to allocate connection");
        return NULL;
    }
    
    conn->client = client;
    conn->db = NULL;
    conn->is_closing = false;
    
    // 设置 socket 为阻塞模式
    infra_error_t err = infra_net_set_nonblock(client, false);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set socket to blocking mode");
        infra_free(conn);
        return NULL;
    }
    
    // 设置较长的超时时间
    struct timeval timeout;
    timeout.tv_sec = 30;  // 30 秒超时
    timeout.tv_usec = 0;
    
    int fd = infra_net_get_fd(client);
    if (fd < 0) {
        INFRA_LOG_ERROR("Failed to get socket file descriptor");
        infra_free(conn);
        return NULL;
    }
    
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        INFRA_LOG_ERROR("Failed to set receive timeout");
        infra_free(conn);
        return NULL;
    }
    
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        INFRA_LOG_ERROR("Failed to set send timeout");
        infra_free(conn);
        return NULL;
    }
    
    // Open database connection using poly_db with WAL mode
    poly_db_config_t config = {
        .type = POLY_DB_TYPE_SQLITE,
        .url = g_service.db_path,
        .max_memory = 100 * 1024 * 1024,  // 100MB 内存限制
        .read_only = false,
        .plugin_path = NULL,
        .allow_fallback = false
    };
    
    INFRA_LOG_INFO("Opening database: %s", g_service.db_path);
    
    err = poly_db_open(&config, &conn->db);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to open database connection: %d", err);
        infra_free(conn);
        return NULL;
    }

    // 启用 WAL 模式以提高并发性能
    err = poly_db_exec(conn->db, "PRAGMA journal_mode=WAL;");
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to enable WAL mode");
        poly_db_close(conn->db);
        infra_free(conn);
        return NULL;
    }

    // 设置较短的超时和重试
    err = poly_db_exec(conn->db, "PRAGMA busy_timeout=5000;");
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set busy timeout");
        poly_db_close(conn->db);
        infra_free(conn);
        return NULL;
    }

    // 设置共享缓存模式
    err = poly_db_exec(conn->db, "PRAGMA cache_size=2000;");  // 2000 pages = ~8MB cache
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set cache size");
        poly_db_close(conn->db);
        infra_free(conn);
        return NULL;
    }

    err = poly_db_exec(conn->db, "PRAGMA synchronous=NORMAL;");  // 提高写入性能
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set synchronous mode");
        poly_db_close(conn->db);
        infra_free(conn);
        return NULL;
    }
    
    INFRA_LOG_INFO("Database connection established");
    return conn;
}

static void sqlite3_conn_destroy(sqlite3_conn_t* conn) {
    if (!conn) {
        INFRA_LOG_ERROR("Attempting to destroy NULL connection");
        return;
    }

    if (conn->is_closing) {
        INFRA_LOG_DEBUG("Connection already being destroyed");
        return;
    }
    conn->is_closing = true;

    INFRA_LOG_DEBUG("Destroying connection: client=%p, db=%p", conn->client, conn->db);

    // Close database connection using poly_db
    if (conn->db) {
        INFRA_LOG_DEBUG("Closing database connection");
        poly_db_close(conn->db);
        conn->db = NULL;
    }

    if (conn->client) {
        INFRA_LOG_DEBUG("Clearing client socket reference");
        conn->client = NULL;
    }

    // Verify cleanup
    if (conn->db != NULL) {
        INFRA_LOG_ERROR("Database connection not properly cleaned up");
    }
    if (conn->client != NULL) {
        INFRA_LOG_ERROR("Client socket reference not properly cleaned up");
    }

    INFRA_LOG_DEBUG("Freeing connection structure");
    infra_free(conn);
}

//-----------------------------------------------------------------------------
// Request handling
//-----------------------------------------------------------------------------

static void handle_request_wrapper(void* args) {
    if (!args) {
        INFRA_LOG_ERROR("NULL handler args");
        return;
    }

    poly_poll_handler_args_t* handler_args = (poly_poll_handler_args_t*)args;
    
    // Get client address
    char client_addr[POLY_MAX_ADDR_LEN] = {0};
    infra_net_addr_t addr = {0};
    
    infra_error_t err = infra_net_getpeername(handler_args->client, &addr);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to get peer address");
        free(handler_args);
        return;
    }
    
    infra_net_addr_to_str(&addr, client_addr, sizeof(client_addr));
    INFRA_LOG_INFO("New connection from %s", client_addr);

    // Create connection state
    sqlite3_conn_t* conn = sqlite3_conn_create(handler_args->client);
    if (!conn) {
        const char* error_msg = "ERROR: Failed to create connection\n";
        size_t sent;
        infra_net_send(handler_args->client, error_msg, strlen(error_msg), &sent);
        free(handler_args);
        return;
    }

    free(handler_args);
    handler_args = NULL;

    INFRA_LOG_INFO("Client connected from %s", client_addr);

    // Process requests
    while (g_service.running) {
        size_t received = 0;
        memset(conn->buffer, 0, sizeof(conn->buffer));  // 清空缓冲区
        
        INFRA_LOG_DEBUG("Waiting for SQL from %s", client_addr);
        err = infra_net_recv(conn->client, conn->buffer, sizeof(conn->buffer) - 1, &received);
        
        if (err == INFRA_ERROR_TIMEOUT) {
            INFRA_LOG_DEBUG("Receive timeout from %s, continuing...", client_addr);
            continue;
        }
        
        if (err != INFRA_OK || received == 0) {
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to receive from %s: %d", client_addr, err);
            } else {
                INFRA_LOG_INFO("Client disconnected: %s", client_addr);
            }
            break;
        }

        // Ensure NULL termination
        conn->buffer[received] = '\0';
        INFRA_LOG_DEBUG("Received SQL from %s (%zu bytes): %s", client_addr, received, conn->buffer);

        // 尝试执行 SQL
        const char* sql = conn->buffer;
        bool is_query = false;

        // 简单检查是否是查询语句
        const char* sql_upper = sql;
        while (*sql_upper && isspace(*sql_upper)) sql_upper++;
        is_query = (strncasecmp(sql_upper, "SELECT", 6) == 0);

        char response[4096] = {0};
        if (is_query) {
            // 执行查询
            poly_db_result_t* result = NULL;
            err = poly_db_query(conn->db, sql, &result);
            if (err != INFRA_OK) {
                snprintf(response, sizeof(response), "ERROR: Query failed (%d)\n", err);
                INFRA_LOG_ERROR("Query failed for %s: %d", client_addr, err);
            } else {
                size_t row_count = 0;
                err = poly_db_result_row_count(result, &row_count);
                if (err != INFRA_OK) {
                    snprintf(response, sizeof(response), "ERROR: Failed to get row count (%d)\n", err);
                    INFRA_LOG_ERROR("Failed to get row count for %s: %d", client_addr, err);
                } else {
                    snprintf(response, sizeof(response), "OK: %zu rows\n", row_count);
                    INFRA_LOG_DEBUG("Query returned %zu rows for %s", row_count, client_addr);
                }
                if (result) {
                    poly_db_result_free(result);
                }
            }
        } else {
            // 执行非查询语句
            err = poly_db_exec(conn->db, sql);
            if (err != INFRA_OK) {
                snprintf(response, sizeof(response), "ERROR: Execution failed (%d)\n", err);
                INFRA_LOG_ERROR("Execution failed for %s: %d", client_addr, err);
            } else {
                snprintf(response, sizeof(response), "OK\n");
                INFRA_LOG_DEBUG("Execution succeeded for %s", client_addr);
            }
        }

        // 立即发送响应
        size_t total_sent = 0;
        size_t response_len = strlen(response);
        
        INFRA_LOG_DEBUG("Sending response to %s (%zu bytes): %s", client_addr, response_len, response);
        
        while (total_sent < response_len) {
            size_t remaining = response_len - total_sent;
            size_t sent = 0;
            
            err = infra_net_send(conn->client, response + total_sent, remaining, &sent);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send response to %s: %d", client_addr, err);
                goto cleanup;
            }
            
            total_sent += sent;
            if (sent == 0) {
                INFRA_LOG_ERROR("Connection closed while sending response to %s", client_addr);
                goto cleanup;
            }
        }
        
        INFRA_LOG_DEBUG("Response sent to %s", client_addr);
    }

cleanup:
    INFRA_LOG_INFO("Closing connection from %s", client_addr);
    sqlite3_conn_destroy(conn);
    INFRA_LOG_DEBUG("Connection cleanup completed for %s", client_addr);
}

//-----------------------------------------------------------------------------
// Service interface
//-----------------------------------------------------------------------------

infra_error_t sqlite3_init(const infra_config_t* config) {
    // 检查服务状态
    if (g_sqlite3_service.state != SERVICE_STATE_UNKNOWN && 
        g_sqlite3_service.state != SERVICE_STATE_STOPPED) {
        INFRA_LOG_ERROR("Service is in invalid state: %d", g_sqlite3_service.state);
        return INFRA_ERROR_INVALID_STATE;
    }

    infra_error_t err = infra_mutex_create(&g_service.mutex);
    if (err != INFRA_OK) {
        return err;
    }

    // 如果没有指定数据库路径，使用临时文件而不是内存数据库
    if (!g_service.db_path[0]) {
        strncpy(g_service.db_path, "/tmp/ppdb_sqlite3.db", sizeof(g_service.db_path) - 1);
    }

    // 更新服务状态
    g_sqlite3_service.state = SERVICE_STATE_STOPPED;
    return INFRA_OK;
}

infra_error_t sqlite3_start(void) {
    // 检查服务状态
    if (g_sqlite3_service.state != SERVICE_STATE_STOPPED) {
        INFRA_LOG_ERROR("Service is in invalid state: %d", g_sqlite3_service.state);
        return INFRA_ERROR_INVALID_STATE;
    }

    // 更新服务状态
    g_sqlite3_service.state = SERVICE_STATE_STARTING;

    // Initialize poll context
    poly_poll_config_t poll_config = {
        .min_threads = 1,
        .max_threads = 4,
        .queue_size = 1000,
        .max_listeners = 1
    };

    g_service.poll_ctx = (poly_poll_context_t*)infra_malloc(sizeof(poly_poll_context_t));
    if (!g_service.poll_ctx) {
        g_sqlite3_service.state = SERVICE_STATE_STOPPED;
        return INFRA_ERROR_NO_MEMORY;
    }

    infra_error_t err = poly_poll_init(g_service.poll_ctx, &poll_config);
    if (err != INFRA_OK) {
        infra_free(g_service.poll_ctx);
        g_service.poll_ctx = NULL;
        g_sqlite3_service.state = SERVICE_STATE_STOPPED;
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
        poly_poll_cleanup(g_service.poll_ctx);
        infra_free(g_service.poll_ctx);
        g_service.poll_ctx = NULL;
        g_sqlite3_service.state = SERVICE_STATE_STOPPED;
        return err;
    }

    // Set connection handler
    poly_poll_set_handler(g_service.poll_ctx, handle_request_wrapper);

    // Start polling in a new thread
    g_service.running = true;
    infra_thread_t thread;
    err = infra_thread_create(&thread, (infra_thread_func_t)poly_poll_start, g_service.poll_ctx);
    if (err != INFRA_OK) {
        g_service.running = false;
        poly_poll_cleanup(g_service.poll_ctx);
        infra_free(g_service.poll_ctx);
        g_service.poll_ctx = NULL;
        g_sqlite3_service.state = SERVICE_STATE_STOPPED;
        return err;
    }

    // 等待服务启动
    infra_sleep(100);  // 等待100ms让服务启动

    // 更新服务状态
    g_sqlite3_service.state = SERVICE_STATE_RUNNING;

    // 等待服务停止
    while (g_service.running) {
        infra_sleep(100);  // 每100ms检查一次
    }

    return INFRA_OK;
}

infra_error_t sqlite3_stop(void) {
    // 检查服务状态
    if (g_sqlite3_service.state != SERVICE_STATE_RUNNING) {
        INFRA_LOG_ERROR("Service is not running");
        return INFRA_ERROR_INVALID_STATE;
    }

    // 更新服务状态
    g_sqlite3_service.state = SERVICE_STATE_STOPPING;

    if (!g_service.running) {
        g_sqlite3_service.state = SERVICE_STATE_STOPPED;
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

    // 更新服务状态
    g_sqlite3_service.state = SERVICE_STATE_STOPPED;
    
    return INFRA_OK;
}

infra_error_t sqlite3_cleanup(void) {
    // 检查服务状态
    if (g_sqlite3_service.state == SERVICE_STATE_RUNNING ||
        g_sqlite3_service.state == SERVICE_STATE_STARTING) {
        INFRA_LOG_ERROR("Cannot cleanup while service is running");
        return INFRA_ERROR_INVALID_STATE;
    }

    // 如果服务正在停止，等待它完成
    if (g_sqlite3_service.state == SERVICE_STATE_STOPPING) {
        INFRA_LOG_ERROR("Service is still stopping");
        return INFRA_ERROR_BUSY;
    }

    // 停止服务（如果还在运行）
    if (g_service.running) {
        sqlite3_stop();
    }

    infra_mutex_destroy(g_service.mutex);

    // 更新服务状态
    g_sqlite3_service.state = SERVICE_STATE_UNKNOWN;
    return INFRA_OK;
}

bool sqlite3_is_running(void) {
    return g_service.running;
}

//-----------------------------------------------------------------------------
// Command handler
//-----------------------------------------------------------------------------

infra_error_t sqlite3_cmd_handler(int argc, char** argv) {
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
