#include "peer_service.h"
#include "../infra/infra_core.h"
#include "../infra/infra_net.h"
#include "../infra/infra_sync.h"
#include "../infra/infra_memory.h"
#include "../infra/infra_error.h"
#include "../poly/poly_db.h"
#include "../poly/poly_poll.h"

//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------

static infra_error_t memkv_init(const infra_config_t* config);
static infra_error_t memkv_cleanup(void);
static infra_error_t memkv_start(void);
static infra_error_t memkv_stop(void);
static bool memkv_is_running(void);
static infra_error_t memkv_cmd_handler(int argc, char* argv[]);

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define MEMKV_VERSION "1.0.0"
#define MEMKV_BUFFER_SIZE 8192
#define MEMKV_DEFAULT_PORT 11211
#define MEMKV_MAX_THREADS 32

// 错误码定义
#define MEMKV_OK INFRA_OK
#define MEMKV_ERROR INFRA_ERROR

// Command Line Options
static const poly_cmd_option_t memkv_options[] = {
    {"port", "Server port", true},
    {"start", "Start the service", false},
    {"stop", "Stop the service", false},
    {"status", "Show service status", false},
    {"engine", "Storage engine (sqlite/duckdb)", true},
    {"plugin", "Plugin path for duckdb", false}
};

static const size_t memkv_option_count = sizeof(memkv_options) / sizeof(memkv_options[0]);

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

typedef struct {
    infra_socket_t sock;
    poly_db_t* store;
    char* rx_buf;
    size_t rx_len;
    bool should_close;
} memkv_conn_t;

typedef struct {
    int port;
    char* engine;
    char* plugin;
    bool running;
    poly_poll_context_t* poll_ctx;
} memkv_config_t;

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

static memkv_config_t g_config = {0};
static bool g_initialized = false;

// 服务实例
peer_service_t g_memkv_service = {
    .config = {
        .name = "memkv",
        .type = SERVICE_TYPE_MEMKV,
        .options = memkv_options,
        .option_count = memkv_option_count,
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
// Helper Functions
//-----------------------------------------------------------------------------

static infra_error_t db_init(poly_db_t** db) {
    poly_db_config_t config = {
        .type = strcmp(g_config.engine, "duckdb") == 0 ? 
                POLY_DB_TYPE_DUCKDB : POLY_DB_TYPE_SQLITE,
        .url = g_config.plugin ? g_config.plugin : ":memory:",
        .max_memory = 0,
        .read_only = false,
        .plugin_path = g_config.plugin,
        .allow_fallback = true
    };

    infra_error_t err = poly_db_open(&config, db);
    if (err != INFRA_OK) return err;

    // 创建 KV 表
    const char* sql = 
        "CREATE TABLE IF NOT EXISTS kv_store ("
        "  key TEXT PRIMARY KEY,"
        "  value BLOB,"
        "  flags INTEGER,"
        "  expiry INTEGER"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_expiry ON kv_store(expiry);";
    
    return poly_db_exec(*db, sql);
}

static infra_error_t kv_get(poly_db_t* db, const char* key, void** value, 
                           size_t* value_len, uint32_t* flags) {
    const char* sql = 
        "SELECT value, flags FROM kv_store WHERE key = ? "
        "AND (expiry = 0 OR expiry > strftime('%s', 'now'))";
    
    poly_db_stmt_t* stmt = NULL;
    infra_error_t err = poly_db_prepare(db, sql, &stmt);
    if (err != INFRA_OK) return err;

    err = poly_db_bind_text(stmt, 1, key, strlen(key));
    if (err != INFRA_OK) {
        poly_db_stmt_finalize(stmt);
        return err;
    }

    err = poly_db_stmt_step(stmt);
    if (err != INFRA_OK) {
        poly_db_stmt_finalize(stmt);
        return err;
    }

    err = poly_db_column_blob(stmt, 0, value, value_len);
    if (err == INFRA_OK && flags) {
        char* flags_data;
        if (poly_db_column_text(stmt, 1, &flags_data) == INFRA_OK && flags_data) {
            memcpy(flags, flags_data, sizeof(*flags));
        }
    }

    poly_db_stmt_finalize(stmt);
    return err;
}

static infra_error_t kv_set(poly_db_t* db, const char* key, const void* value, 
                           size_t value_len, uint32_t flags, time_t expiry) {
    const char* sql = 
        "INSERT OR REPLACE INTO kv_store (key, value, flags, expiry) VALUES (?, ?, ?, ?)";
    
    poly_db_stmt_t* stmt = NULL;
    infra_error_t err = poly_db_prepare(db, sql, &stmt);
    if (err != INFRA_OK) return err;

    err = poly_db_bind_text(stmt, 1, key, strlen(key));
    if (err != INFRA_OK) goto cleanup;

    err = poly_db_bind_blob(stmt, 2, value, value_len);
    if (err != INFRA_OK) goto cleanup;

    err = poly_db_bind_text(stmt, 3, (const char*)&flags, sizeof(flags));
    if (err != INFRA_OK) goto cleanup;

    err = poly_db_bind_text(stmt, 4, (const char*)&expiry, sizeof(expiry));
    if (err != INFRA_OK) goto cleanup;

    err = poly_db_stmt_step(stmt);

cleanup:
    poly_db_stmt_finalize(stmt);
    return err;
}

static void handle_get(memkv_conn_t* conn, const char* key) {
    void* value = NULL;
    size_t value_len = 0;
    uint32_t flags = 0;

    if (kv_get(conn->store, key, &value, &value_len, &flags) == INFRA_OK && value) {
        char header[128];
        int header_len = snprintf(header, sizeof(header), 
                                "VALUE %s %u %zu\r\n", key, flags, value_len);
        
        infra_net_send(conn->sock, header, header_len, NULL);
        infra_net_send(conn->sock, value, value_len, NULL);
        infra_net_send(conn->sock, "\r\n", 2, NULL);
        infra_net_send(conn->sock, "END\r\n", 5, NULL);
        free(value);
    } else {
        infra_net_send(conn->sock, "END\r\n", 5, NULL);
    }
}

static void handle_set(memkv_conn_t* conn, const char* key, const char* flags_str,
                      const char* exptime_str, const char* bytes_str, const char* value) {
    uint32_t flags = strtoul(flags_str, NULL, 10);
    time_t exptime = strtol(exptime_str, NULL, 10);
    size_t bytes = strtoul(bytes_str, NULL, 10);

    if (exptime > 0 && exptime < 2592000) {  // 30天
        exptime += time(NULL);
    }

    if (kv_set(conn->store, key, value, bytes, flags, exptime) == INFRA_OK) {
        infra_net_send(conn->sock, "STORED\r\n", 8, NULL);
    } else {
        infra_net_send(conn->sock, "NOT_STORED\r\n", 12, NULL);
    }
}

static void handle_flush(memkv_conn_t* conn) {
    if (poly_db_exec(conn->store, "DELETE FROM kv_store") == INFRA_OK) {
        infra_net_send(conn->sock, "OK\r\n", 4, NULL);
    } else {
        infra_net_send(conn->sock, "ERROR\r\n", 7, NULL);
    }
}

static void handle_command(memkv_conn_t* conn, char* cmd) {
    char* saveptr = NULL;
    char* token = strtok_r(cmd, " \r\n", &saveptr);
    if (!token) return;

    if (strcasecmp(token, "get") == 0) {
        token = strtok_r(NULL, " \r\n", &saveptr);
        if (token) handle_get(conn, token);
    } 
    else if (strcasecmp(token, "set") == 0) {
        char* key = strtok_r(NULL, " \r\n", &saveptr);
        char* flags = strtok_r(NULL, " \r\n", &saveptr);
        char* exptime = strtok_r(NULL, " \r\n", &saveptr);
        char* bytes = strtok_r(NULL, " \r\n", &saveptr);
        char* value = strtok_r(NULL, "\r\n", &saveptr);

        if (key && flags && exptime && bytes && value) {
            handle_set(conn, key, flags, exptime, bytes, value);
        }
    }
    else if (strcasecmp(token, "flush_all") == 0) {
        handle_flush(conn);
    }
    else {
        infra_net_send(conn->sock, "ERROR\r\n", 7, NULL);
    }
}

static void handle_connection(void* arg) {
    poly_poll_handler_args_t* args = (poly_poll_handler_args_t*)arg;
    if (!args) return;

    memkv_conn_t conn = {
        .sock = args->client,
        .rx_buf = malloc(MEMKV_BUFFER_SIZE),
        .rx_len = 0,
        .should_close = false
    };

    if (!conn.rx_buf) goto cleanup;
    if (db_init(&conn.store) != INFRA_OK) goto cleanup;

    while (!conn.should_close) {
        size_t received = 0;
        infra_error_t err = infra_net_recv(conn.sock, 
                                         conn.rx_buf + conn.rx_len,
                                         MEMKV_BUFFER_SIZE - conn.rx_len - 1,
                                         &received);
        
        if (err != INFRA_OK || received == 0) break;
        
        conn.rx_len += received;
        conn.rx_buf[conn.rx_len] = '\0';

        char* cmd_end;
        while ((cmd_end = strstr(conn.rx_buf, "\r\n")) != NULL) {
            *cmd_end = '\0';
            handle_command(&conn, conn.rx_buf);
            
            // 移动剩余数据
            size_t remaining = conn.rx_len - (cmd_end - conn.rx_buf + 2);
            if (remaining > 0) {
                memmove(conn.rx_buf, cmd_end + 2, remaining);
            }
            conn.rx_len = remaining;
        }
    }

cleanup:
    if (conn.store) poly_db_close(conn.store);
    if (conn.rx_buf) free(conn.rx_buf);
    infra_net_close(conn.sock);
    free(args);
}

//-----------------------------------------------------------------------------
// Service Interface Implementation
//-----------------------------------------------------------------------------

static infra_error_t memkv_init(const infra_config_t* config) {
    if (!config) return INFRA_ERROR_INVALID_PARAM;

    g_config.port = MEMKV_DEFAULT_PORT;
    g_config.engine = "sqlite";
    g_initialized = true;
    
    return INFRA_OK;
}

static infra_error_t memkv_cleanup(void) {
    if (g_config.running) memkv_stop();
    g_initialized = false;
    return INFRA_OK;
}

static infra_error_t memkv_start(void) {
    if (!g_initialized) return INFRA_ERROR_NOT_INITIALIZED;
    if (g_config.running) return INFRA_OK;

    g_memkv_service.state = SERVICE_STATE_STARTING;

    // 创建轮询上下文
    g_config.poll_ctx = malloc(sizeof(poly_poll_context_t));
    if (!g_config.poll_ctx) return INFRA_ERROR_NO_MEMORY;

    poly_poll_config_t poll_config = {
        .min_threads = MEMKV_MAX_THREADS / 2,
        .max_threads = MEMKV_MAX_THREADS,
        .queue_size = 1024,
        .max_listeners = 1
    };

    infra_error_t err = poly_poll_init(g_config.poll_ctx, &poll_config);
    if (err != INFRA_OK) goto cleanup;

    // 添加监听器
    poly_poll_listener_t listener = {
        .bind_port = g_config.port,
        .user_data = NULL
    };
    strncpy(listener.bind_addr, "127.0.0.1", POLY_MAX_ADDR_LEN - 1);

    err = poly_poll_add_listener(g_config.poll_ctx, &listener);
    if (err != INFRA_OK) goto cleanup;

    poly_poll_set_handler(g_config.poll_ctx, handle_connection);
    err = poly_poll_start(g_config.poll_ctx);
    if (err != INFRA_OK) goto cleanup;

    g_config.running = true;
    g_memkv_service.state = SERVICE_STATE_RUNNING;
    return INFRA_OK;

cleanup:
    if (g_config.poll_ctx) {
        poly_poll_cleanup(g_config.poll_ctx);
        free(g_config.poll_ctx);
        g_config.poll_ctx = NULL;
    }
    g_memkv_service.state = SERVICE_STATE_STOPPED;
    return err;
}

static infra_error_t memkv_stop(void) {
    if (!g_initialized || !g_config.running) return INFRA_OK;

    g_memkv_service.state = SERVICE_STATE_STOPPING;

    if (g_config.poll_ctx) {
        poly_poll_stop(g_config.poll_ctx);
        poly_poll_cleanup(g_config.poll_ctx);
        free(g_config.poll_ctx);
        g_config.poll_ctx = NULL;
    }

    g_config.running = false;
    g_memkv_service.state = SERVICE_STATE_STOPPED;
    return INFRA_OK;
}

static bool memkv_is_running(void) {
    return g_initialized && g_config.running;
}

static infra_error_t memkv_cmd_handler(int argc, char* argv[]) {
    if (argc < 2) return INFRA_ERROR_INVALID_PARAM;

    bool start = false, stop = false, status = false;
    const char* config_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--config=", 9) == 0) {
            config_path = argv[i] + 9;
        } else if (strcmp(argv[i], "--start") == 0) {
            start = true;
        } else if (strcmp(argv[i], "--stop") == 0) {
            stop = true;
        } else if (strcmp(argv[i], "--status") == 0) {
            status = true;
        }
    }

    if (start) {
        if (!g_initialized) {
            peer_service_config_t init_config = {
                .name = "memkv",
                .type = SERVICE_TYPE_MEMKV,
                .options = memkv_options,
                .option_count = memkv_option_count,
                .config_path = config_path
            };
            infra_error_t err = memkv_init((const infra_config_t*)&init_config);
            if (err != INFRA_OK) return err;
        }
        return memkv_start();
    } 
    else if (stop) {
        return memkv_stop();
    }
    else if (status) {
        printf("memkv service is %s\n", memkv_is_running() ? "running" : "stopped");
        return INFRA_OK;
    }

    return INFRA_ERROR_INVALID_PARAM;
}
