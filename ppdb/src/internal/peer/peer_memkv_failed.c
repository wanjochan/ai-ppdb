#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_log.h"
#include "internal/infra/infra_thread.h"
#include "internal/poly/poly_db.h"
#include "internal/poly/poly_poll.h"
#include "internal/poly/poly_cmdline.h"
#include "internal/peer/peer_service.h"
#include "internal/peer/peer_memkv.h"
#include <netinet/tcp.h>  // 添加TCP_NODELAY的定义
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/socket.h>
#include <fcntl.h>

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

typedef struct {
    infra_socket_t sock;              // Client socket
    poly_db_t* store;                 // Database connection
    char* rx_buf;                     // Receive buffer
    size_t rx_len;                    // Current buffer length
    bool should_close;                // Connection close flag
    volatile bool is_closing;         // Connection is being destroyed
    volatile bool is_initialized;     // Connection is fully initialized
    uint64_t created_time;           // Connection creation timestamp
    uint64_t last_active_time;       // Last activity timestamp
    size_t total_commands;           // Total commands processed
    size_t failed_commands;          // Failed commands count
    char client_addr[64];            // Client address string
} memkv_conn_t;

typedef struct {
    int port;
    char* engine;
    char* plugin;
    bool running;
    poly_poll_context_t* ctx;
} memkv_config_t;

//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------

infra_error_t memkv_init(void);
infra_error_t memkv_cleanup(void);
infra_error_t memkv_start(void);
infra_error_t memkv_stop(void);
infra_error_t memkv_cmd_handler(const char* cmd, char* response, size_t size);
infra_error_t memkv_apply_config(const poly_service_config_t* config);
static void memkv_conn_destroy(memkv_conn_t* conn);
static void handle_connection_wrapper(void* args);
static void handle_request_wrapper(void* args);

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define MEMKV_VERSION "1.0.0"
#define MEMKV_BUFFER_SIZE (64 * 1024)  // 64KB buffer
#define MEMKV_MAX_DATA_SIZE (32 * 1024 * 1024)  // 32MB max value size
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
// Global Variables
//-----------------------------------------------------------------------------

// Global service instance
peer_service_t g_memkv_service = {
    .config = {
        .name = "memkv",
        .user_data = NULL
    },
    .state = PEER_SERVICE_STATE_INIT,
    .init = memkv_init,
    .cleanup = memkv_cleanup,
    .start = memkv_start,
    .stop = memkv_stop,
    .cmd_handler = memkv_cmd_handler,
    .apply_config = memkv_apply_config
};

// Global state
static struct {
    bool running;
    int port;
    char* engine;
    char* plugin;
    poly_poll_context_t* ctx;
} g_memkv_state = {0};

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

static infra_error_t db_init(poly_db_t** db) {
    if (!db) return INFRA_ERROR_INVALID_PARAM;
    
    poly_db_config_t config = {
        .type = strcmp(g_memkv_state.engine, "duckdb") == 0 ? 
                POLY_DB_TYPE_DUCKDB : POLY_DB_TYPE_SQLITE,
        .url = g_memkv_state.plugin ? g_memkv_state.plugin : ":memory:",
        .max_memory = 0,
        .read_only = false,
        .plugin_path = g_memkv_state.plugin,
        .allow_fallback = true
    };

    infra_error_t err = poly_db_open(&config, db);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to open database: %d", err);
        return err;
    }

    // Create KV table
    const char* sql = 
        "CREATE TABLE IF NOT EXISTS kv_store ("
        "  key TEXT PRIMARY KEY,"
        "  value BLOB,"
        "  flags INTEGER,"
        "  expiry INTEGER"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_expiry ON kv_store(expiry);";
    
    err = poly_db_exec(*db, sql);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create tables: %d", err);
        poly_db_close(*db);
        *db = NULL;
    }
    return err;
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

    char* text_value = NULL;
    err = poly_db_column_text(stmt, 0, &text_value);
    if (err != INFRA_OK) {
        poly_db_stmt_finalize(stmt);
        return err;
    }

    if (text_value) {
        *value = text_value;
        *value_len = strlen(text_value);
        
        if (flags) {
            char* flags_str = NULL;
            if (poly_db_column_text(stmt, 1, &flags_str) == INFRA_OK && flags_str) {
                *flags = (uint32_t)strtoul(flags_str, NULL, 10);
                free(flags_str);
            }
        }
        poly_db_stmt_finalize(stmt);
        return INFRA_OK;
    }

    poly_db_stmt_finalize(stmt);
    return INFRA_ERROR_NOT_FOUND;
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

    char flags_str[32];
    snprintf(flags_str, sizeof(flags_str), "%u", flags);
    err = poly_db_bind_text(stmt, 3, flags_str, strlen(flags_str));
    if (err != INFRA_OK) goto cleanup;

    char expiry_str[32];
    snprintf(expiry_str, sizeof(expiry_str), "%ld", expiry);
    err = poly_db_bind_text(stmt, 4, expiry_str, strlen(expiry_str));
    if (err != INFRA_OK) goto cleanup;

    err = poly_db_stmt_step(stmt);

cleanup:
    poly_db_stmt_finalize(stmt);
    return err;
}

static infra_error_t kv_delete(poly_db_t* db, const char* key) {
    const char* sql = "DELETE FROM kv_store WHERE key = ?";
    poly_db_stmt_t* stmt = NULL;
    
    infra_error_t err = poly_db_prepare(db, sql, &stmt);
    if (err != INFRA_OK) return err;
    
    err = poly_db_bind_text(stmt, 1, key, strlen(key));
    if (err != INFRA_OK) {
        poly_db_stmt_finalize(stmt);
        return err;
    }
    
    err = poly_db_stmt_step(stmt);
    poly_db_stmt_finalize(stmt);
    return err;
}

static infra_error_t kv_flush(poly_db_t* db) {
    return poly_db_exec(db, "DELETE FROM kv_store");
}

static infra_error_t send_all(infra_socket_t sock, const void* data, size_t len) {
    if (sock <= 0 || !data || len == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    const char* buf = (const char*)data;
    size_t sent = 0;
    int retry_count = 0;
    const int max_retries = 3;
    
    while (sent < len) {
        size_t remaining = len - sent;
        size_t bytes_sent = 0;
        
        infra_error_t err = infra_net_send(sock, buf + sent, remaining, &bytes_sent);
        
        if (err == INFRA_ERROR_WOULD_BLOCK) {
            if (retry_count < max_retries) {
                INFRA_LOG_DEBUG("Send would block, retrying (%d/%d)", 
                              retry_count + 1, max_retries);
                usleep(10000); // 10ms backoff
                retry_count++;
                continue;
            }
            INFRA_LOG_ERROR("Send failed after %d retries", max_retries);
            return err;
        }
        
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to send data: %d", err);
            return err;
        }
        
        if (bytes_sent == 0) {
            INFRA_LOG_ERROR("Connection closed by peer");
            return INFRA_ERROR_CLOSED;
        }
        
        sent += bytes_sent;
        retry_count = 0;  // Reset retry counter on successful send
        
        // Log progress for large transfers
        if (len > 65536 && sent % 65536 == 0) {
            INFRA_LOG_DEBUG("Send progress: %zu/%zu bytes (%.1f%%)", 
                          sent, len, (sent * 100.0) / len);
        }
    }
    
    return INFRA_OK;
}

static void handle_get(memkv_conn_t* conn, const char* key) {
    if (!conn || !conn->store || !key || conn->sock <= 0) {
        INFRA_LOG_ERROR("Invalid parameters in handle_get");
        return;
    }

    INFRA_LOG_DEBUG("Handling GET command for key '%s' from %s", key, conn->client_addr);
    
    void* value = NULL;
    size_t value_len = 0;
    uint32_t flags = 0;
    
    infra_error_t err = kv_get(conn->store, key, &value, &value_len, &flags);
    if (err != INFRA_OK) {
        if (err == INFRA_ERROR_NOT_FOUND) {
            INFRA_LOG_DEBUG("Key '%s' not found", key);
            err = send_all(conn->sock, "END\r\n", 5);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send END response: %d", err);
                conn->should_close = true;
            }
            return;
        }
        INFRA_LOG_ERROR("Failed to get value for key '%s': %d", key, err);
        conn->failed_commands++;
        err = send_all(conn->sock, "SERVER_ERROR\r\n", 14);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to send error response: %d", err);
            conn->should_close = true;
        }
        return;
    }

    // Format: VALUE <key> <flags> <bytes>\r\n<data>\r\nEND\r\n
    char header[512];
    int header_len = snprintf(header, sizeof(header), 
        "VALUE %s %u %zu\r\n", key, flags, value_len);
    if (header_len < 0 || header_len >= (int)sizeof(header)) {
        INFRA_LOG_ERROR("Failed to format response header");
        conn->failed_commands++;
        if (value) free(value);
        err = send_all(conn->sock, "SERVER_ERROR\r\n", 14);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to send error response: %d", err);
            conn->should_close = true;
        }
        return;
    }

    // Send header
    err = send_all(conn->sock, header, header_len);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to send response header: %d", err);
        conn->failed_commands++;
        if (value) free(value);
        conn->should_close = true;
        return;
    }

    // Send value
    if (value && value_len > 0) {
        err = send_all(conn->sock, value, value_len);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to send value: %d", err);
            conn->failed_commands++;
            free(value);
            conn->should_close = true;
            return;
        }
    }

    // Send \r\n after value
    err = send_all(conn->sock, "\r\n", 2);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to send value terminator: %d", err);
        conn->failed_commands++;
        if (value) free(value);
        conn->should_close = true;
        return;
    }

    // Send END
    err = send_all(conn->sock, "END\r\n", 5);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to send END marker: %d", err);
        conn->failed_commands++;
        if (value) free(value);
        conn->should_close = true;
        return;
    }

    if (value) free(value);
    INFRA_LOG_DEBUG("Successfully sent value for key '%s'", key);
    conn->last_active_time = time(NULL);
}

static void handle_set(memkv_conn_t* conn, const char* key, 
                      const char* flags_str, const char* exptime_str, 
                      const char* bytes_str, bool noreply) {
    if (!conn || !key || !flags_str || !exptime_str || !bytes_str) {
        INFRA_LOG_ERROR("Invalid parameters");
        return;
    }

    INFRA_LOG_DEBUG("Handling SET command for key '%s' from %s", key, conn->client_addr);

    // Parse parameters
    char* endptr;
    uint32_t flags = strtoul(flags_str, &endptr, 10);
    if (*endptr != '\0') {
        INFRA_LOG_ERROR("Invalid flags value: %s", flags_str);
        conn->failed_commands++;
        if (!noreply) {
            infra_error_t err = send_all(conn->sock, "CLIENT_ERROR invalid flags\r\n", 25);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send error response: %d", err);
                conn->should_close = true;
            }
        }
        return;
    }

    long exptime = strtol(exptime_str, &endptr, 10);
    if (*endptr != '\0') {
        INFRA_LOG_ERROR("Invalid exptime value: %s", exptime_str);
        conn->failed_commands++;
        if (!noreply) {
            infra_error_t err = send_all(conn->sock, "CLIENT_ERROR invalid exptime\r\n", 27);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send error response: %d", err);
                conn->should_close = true;
            }
        }
        return;
    }

    size_t bytes = strtoul(bytes_str, &endptr, 10);
    if (*endptr != '\0' || bytes > MEMKV_MAX_DATA_SIZE) {
        INFRA_LOG_ERROR("Invalid bytes value: %s", bytes_str);
        conn->failed_commands++;
        if (!noreply) {
            infra_error_t err = send_all(conn->sock, "CLIENT_ERROR invalid bytes\r\n", 25);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send error response: %d", err);
                conn->should_close = true;
            }
        }
        return;
    }

    // Wait for data line
    char* data = (char*)infra_malloc(bytes + 2); // +2 for \r\n
    if (!data) {
        INFRA_LOG_ERROR("Failed to allocate memory for data");
        conn->failed_commands++;
        if (!noreply) {
            infra_error_t err = send_all(conn->sock, "SERVER_ERROR out of memory\r\n", 26);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send error response: %d", err);
                conn->should_close = true;
            }
        }
        return;
    }

    size_t received = 0;
    infra_error_t err = infra_net_recv(conn->sock, data, bytes + 2, &received);
    if (err != INFRA_OK || received != bytes + 2 || 
        data[bytes] != '\r' || data[bytes + 1] != '\n') {
        INFRA_LOG_ERROR("Failed to receive data or invalid format");
        conn->failed_commands++;
        infra_free(data);
        if (!noreply) {
            err = send_all(conn->sock, "CLIENT_ERROR bad data chunk\r\n", 26);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send error response: %d", err);
                conn->should_close = true;
            }
        }
        return;
    }

    // Convert relative exptime to absolute
    uint64_t abs_exptime = 0;
    if (exptime > 0) {
        if (exptime > 60*60*24*30) { // > 30 days
            abs_exptime = exptime;
        } else {
            abs_exptime = time(NULL) + exptime;
        }
    }

    // Store the value
    err = kv_set(conn->store, key, data, bytes, flags, abs_exptime);
    infra_free(data);

    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to store value: %d", err);
        conn->failed_commands++;
        if (!noreply) {
            err = send_all(conn->sock, "SERVER_ERROR\r\n", 14);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send error response: %d", err);
                conn->should_close = true;
            }
        }
        return;
    }

    if (!noreply) {
        err = send_all(conn->sock, "STORED\r\n", 8);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to send success response: %d", err);
            conn->should_close = true;
            return;
        }
    }

    INFRA_LOG_DEBUG("Successfully stored value for key '%s'", key);
}

static void handle_delete(memkv_conn_t* conn, const char* key, bool noreply) {
    if (!conn || !conn->store || !key || conn->sock <= 0) {
        INFRA_LOG_ERROR("Invalid parameters in handle_delete");
        if (!noreply && conn && conn->sock > 0) {
            infra_net_send(conn->sock, "CLIENT_ERROR bad command line format\r\n", 37, NULL);
        }
        return;
    }

    // 检查 key 的长度
    size_t key_len = strlen(key);
    if (key_len == 0 || key_len > 250) {  // 250 是一个合理的限制
        INFRA_LOG_ERROR("Invalid key length: %zu", key_len);
        if (!noreply && conn->sock > 0) {
            infra_net_send(conn->sock, "CLIENT_ERROR invalid key length\r\n", 31, NULL);
        }
        return;
    }

    INFRA_LOG_DEBUG("Handling DELETE command for key='%s'", key);
    
    const char* sql = "DELETE FROM kv_store WHERE key = ?";
    poly_db_stmt_t* stmt = NULL;
    
    infra_error_t err = poly_db_prepare(conn->store, sql, &stmt);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to prepare DELETE statement: %d", err);
        if (!noreply && conn->sock > 0) {
            infra_net_send(conn->sock, "ERROR\r\n", 7, NULL);
        }
        return;
    }
    
    err = poly_db_bind_text(stmt, 1, key, key_len);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to bind key to DELETE statement: %d", err);
        poly_db_stmt_finalize(stmt);
        if (!noreply && conn->sock > 0) {
            infra_net_send(conn->sock, "ERROR\r\n", 7, NULL);
        }
        return;
    }
    
    err = poly_db_stmt_step(stmt);
    poly_db_stmt_finalize(stmt);
    
    if (!noreply && conn->sock > 0) {
        if (err == INFRA_OK) {
            err = send_all(conn->sock, "DELETED\r\n", 9);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send DELETED response: %d", err);
                conn->should_close = true;
            }
        } else {
            err = send_all(conn->sock, "NOT_FOUND\r\n", 11);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send NOT_FOUND response: %d", err);
                conn->should_close = true;
            }
        }
    }
}

static void handle_flush(memkv_conn_t* conn, bool noreply) {
    if (!conn || !conn->store || conn->sock <= 0) {
        INFRA_LOG_ERROR("Invalid parameters in handle_flush");
        if (!noreply && conn && conn->sock > 0) {
            infra_net_send(conn->sock, "CLIENT_ERROR bad command line format\r\n", 37, NULL);
        }
        return;
    }

    INFRA_LOG_DEBUG("Handling FLUSH_ALL command");
    
    // 使用事务来确保原子性
    infra_error_t err = poly_db_exec(conn->store, "BEGIN TRANSACTION");
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to begin transaction: %d", err);
        if (!noreply && conn->sock > 0) {
            infra_net_send(conn->sock, "ERROR\r\n", 7, NULL);
        }
        return;
    }
    
    err = poly_db_exec(conn->store, "DELETE FROM kv_store");
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to execute FLUSH_ALL: %d", err);
        poly_db_exec(conn->store, "ROLLBACK");
        if (!noreply && conn->sock > 0) {
            infra_net_send(conn->sock, "ERROR\r\n", 7, NULL);
        }
        return;
    }

    err = poly_db_exec(conn->store, "COMMIT");
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to commit transaction: %d", err);
        poly_db_exec(conn->store, "ROLLBACK");
        if (!noreply && conn->sock > 0) {
            infra_net_send(conn->sock, "ERROR\r\n", 7, NULL);
        }
        return;
    }

    if (!noreply && conn->sock > 0) {
        err = send_all(conn->sock, "OK\r\n", 4);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to send OK response: %d", err);
            conn->should_close = true;
        }
    }
}

static void handle_incr_decr(memkv_conn_t* conn, const char* key, const char* value_str, bool is_incr) {
    if (!conn || !conn->store || !key || !value_str) {
        INFRA_LOG_ERROR("Invalid parameters in handle_incr_decr");
        infra_net_send(conn->sock, "CLIENT_ERROR bad command line format\r\n", 37, NULL);
        return;
    }

    INFRA_LOG_DEBUG("Handling %s command for key='%s', value='%s'", 
                    is_incr ? "INCR" : "DECR", key, value_str);
           
    uint64_t delta = strtoull(value_str, NULL, 10);
    void* old_value = NULL;
    size_t old_value_len = 0;
    uint32_t flags = 0;
    
    infra_error_t err = kv_get(conn->store, key, &old_value, &old_value_len, &flags);
    if (err != INFRA_OK || !old_value) {
        if (is_incr) {
            // 对于INCR，如果key不存在，初始化为0
            char zero_str[] = "0";
            err = kv_set(conn->store, key, zero_str, strlen(zero_str), 0, 0);
            if (err == INFRA_OK) {
                err = send_all(conn->sock, "0\r\n", 3);
                if (err != INFRA_OK) {
                    INFRA_LOG_ERROR("Failed to send initial value response: %d", err);
                    conn->should_close = true;
                }
            } else {
                INFRA_LOG_ERROR("Failed to set initial value: %d", err);
                err = send_all(conn->sock, "ERROR\r\n", 7);
                if (err != INFRA_OK) {
                    INFRA_LOG_ERROR("Failed to send ERROR response: %d", err);
                    conn->should_close = true;
                }
            }
        } else {
            err = send_all(conn->sock, "NOT_FOUND\r\n", 11);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send NOT_FOUND response: %d", err);
                conn->should_close = true;
            }
        }
        if (old_value) free(old_value);
        return;
    }
    
    // 确保old_value是以null结尾的字符串
    char* null_term_value = malloc(old_value_len + 1);
    if (!null_term_value) {
        INFRA_LOG_ERROR("Failed to allocate memory for null-terminated value");
        free(old_value);
        err = send_all(conn->sock, "SERVER_ERROR out of memory\r\n", 26);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to send error response: %d", err);
            conn->should_close = true;
        }
        return;
    }
    memcpy(null_term_value, old_value, old_value_len);
    null_term_value[old_value_len] = '\0';
    
    uint64_t current = strtoull(null_term_value, NULL, 10);
    free(old_value);
    free(null_term_value);
    
    if (is_incr) {
        // Check for overflow
        if (current > UINT64_MAX - delta) {
            INFRA_LOG_ERROR("Increment would cause overflow");
            err = send_all(conn->sock, "ERROR\r\n", 7);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send ERROR response: %d", err);
                conn->should_close = true;
            }
            return;
        }
        current += delta;
    } else {
        if (current < delta) {
            current = 0;
        } else {
            current -= delta;
        }
    }
    
    char new_value[32];
    int new_value_len = snprintf(new_value, sizeof(new_value), "%lu", current);
    if (new_value_len < 0 || new_value_len >= (int)sizeof(new_value)) {
        INFRA_LOG_ERROR("Failed to format new value");
        err = send_all(conn->sock, "ERROR\r\n", 7);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to send ERROR response: %d", err);
            conn->should_close = true;
        }
        return;
    }
    
    err = kv_set(conn->store, key, new_value, new_value_len, flags, 0);
    if (err == INFRA_OK) {
        char response[32];
        int response_len = snprintf(response, sizeof(response), "%lu\r\n", current);
        if (response_len < 0 || response_len >= (int)sizeof(response)) {
            INFRA_LOG_ERROR("Failed to format response");
            err = send_all(conn->sock, "ERROR\r\n", 7);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send ERROR response: %d", err);
                conn->should_close = true;
            }
            return;
        }
        
        err = send_all(conn->sock, response, response_len);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to send response: %d", err);
            conn->should_close = true;
        }
    } else {
        INFRA_LOG_ERROR("Failed to store new value: %d", err);
        err = send_all(conn->sock, "ERROR\r\n", 7);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to send ERROR response: %d", err);
            conn->should_close = true;
        }
    }
}

// Forward declarations
static void handle_connection_wrapper(void* args);
static void handle_request_wrapper(void* args);

static void handle_connection(poly_poll_handler_args_t* args) {
    if (!args) {
        INFRA_LOG_ERROR("NULL handler args");
        return;
    }

    infra_socket_t client = args->client;
    if (client <= 0) {
        INFRA_LOG_ERROR("Invalid client socket");
        return;
    }
    
    // Get client address for logging
    infra_net_addr_t addr;
    memset(&addr, 0, sizeof(addr));
    infra_error_t err = infra_net_get_peer_addr(client, &addr);
    char client_addr[64] = "unknown";
    if (err == INFRA_OK) {
        infra_net_addr_to_string(&addr, client_addr, sizeof(client_addr));
        INFRA_LOG_INFO("New client connection from %s", client_addr);
    } else {
        INFRA_LOG_ERROR("Failed to get peer address: %d", err);
    }

    // Create connection context
    memkv_conn_t* conn = (memkv_conn_t*)infra_malloc(sizeof(memkv_conn_t));
    if (!conn) {
        INFRA_LOG_ERROR("Failed to allocate connection context");
        infra_net_close(client);
        return;
    }

    // Initialize connection
    memset(conn, 0, sizeof(memkv_conn_t));
    conn->sock = client;
    conn->should_close = false;
    conn->is_closing = false;
    conn->is_initialized = false;
    conn->created_time = time(NULL);
    conn->last_active_time = conn->created_time;
    strncpy(conn->client_addr, client_addr, sizeof(conn->client_addr) - 1);
    conn->client_addr[sizeof(conn->client_addr) - 1] = '\0';
    
    // 设置非阻塞模式
    err = infra_net_set_nonblock(client, true);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set socket to non-blocking mode");
        infra_free(conn);
        infra_net_close(client);
        return;
    }
    
    // 设置socket选项
    int flag = 1;
    if (setsockopt(client, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
        INFRA_LOG_ERROR("Failed to set TCP_NODELAY");
        infra_free(conn);
        infra_net_close(client);
        return;
    }

    // 设置 TCP keepalive
#ifdef __APPLE__
    // macOS 使用不同的 socket 选项
    int keepalive = 1;
    if (setsockopt(client, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0) {
        INFRA_LOG_ERROR("Failed to set SO_KEEPALIVE");
        infra_free(conn);
        infra_net_close(client);
        return;
    }
#else
    int keepalive_time = 60;  // 60 秒后开始发送 keepalive
    int keepalive_intvl = 10;  // 每 10 秒发送一次
    int keepalive_probes = 5;  // 最多发送 5 次

    if (setsockopt(client, IPPROTO_TCP, TCP_KEEPIDLE, &keepalive_time, sizeof(keepalive_time)) < 0) {
        INFRA_LOG_ERROR("Failed to set TCP_KEEPIDLE");
        infra_free(conn);
        infra_net_close(client);
        return;
    }
    if (setsockopt(client, IPPROTO_TCP, TCP_KEEPINTVL, &keepalive_intvl, sizeof(keepalive_intvl)) < 0) {
        INFRA_LOG_ERROR("Failed to set TCP_KEEPINTVL");
        infra_free(conn);
        infra_net_close(client);
        return;
    }
    if (setsockopt(client, IPPROTO_TCP, TCP_KEEPCNT, &keepalive_probes, sizeof(keepalive_probes)) < 0) {
        INFRA_LOG_ERROR("Failed to set TCP_KEEPCNT");
        infra_free(conn);
        infra_net_close(client);
        return;
    }
#endif

    // 设置发送和接收缓冲区大小
    int buf_size = 64 * 1024;  // 64KB
    if (setsockopt(client, SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(buf_size)) < 0) {
        INFRA_LOG_ERROR("Failed to set SO_RCVBUF");
        infra_free(conn);
        infra_net_close(client);
        return;
    }
    if (setsockopt(client, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size)) < 0) {
        INFRA_LOG_ERROR("Failed to set SO_SNDBUF");
        infra_free(conn);
        infra_net_close(client);
        return;
    }

    // Initialize database connection
    poly_db_config_t db_config = {0};  // 确保完全初始化
    db_config.type = (g_memkv_state.engine && strcmp(g_memkv_state.engine, "duckdb") == 0) ? 
                    POLY_DB_TYPE_DUCKDB : POLY_DB_TYPE_SQLITE;
    
    // 使用临时变量存储 plugin 路径,避免悬空指针
    const char* plugin_path = g_memkv_state.plugin ? g_memkv_state.plugin : ":memory:";
    char* url = strdup(plugin_path);  // 使用非 const 变量
    if (!url) {
        INFRA_LOG_ERROR("Failed to allocate database URL");
        infra_free(conn);
        infra_net_close(client);
        return;
    }
    db_config.url = url;
    db_config.max_memory = 100 * 1024 * 1024;  // 100MB memory limit
    db_config.read_only = false;
    db_config.plugin_path = url; // 使用同一份拷贝
    db_config.allow_fallback = true;

    err = poly_db_open(&db_config, &conn->store);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize database connection: %d", err);
        free(url);
        infra_free(conn);
        infra_net_close(client);
        return;
    }
    free(url);

    // 设置数据库优化选项
    const char* db_options[] = {
        "PRAGMA journal_mode=WAL;",
        "PRAGMA synchronous=NORMAL;",
        "PRAGMA cache_size=2000;",
        "PRAGMA busy_timeout=5000;",
        "PRAGMA temp_store=MEMORY;",
        NULL
    };

    for (const char** opt = db_options; *opt; opt++) {
        err = poly_db_exec(conn->store, *opt);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to set database option '%s': %d", *opt, err);
            if (conn->store) {
                poly_db_close(conn->store);
            }
            infra_free(conn);
            infra_net_close(client);
            return;
        }
    }

    // Create table if not exists
    err = poly_db_exec(conn->store, 
        "CREATE TABLE IF NOT EXISTS kv_store ("
        "key TEXT PRIMARY KEY,"
        "value BLOB,"
        "flags INTEGER,"
        "expiry INTEGER"
        ")");
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create table: %d", err);
        if (conn->store) {
            poly_db_close(conn->store);
        }
        infra_free(conn);
        infra_net_close(client);
        return;
    }

    // Allocate receive buffer
    conn->rx_buf = (char*)infra_malloc(MEMKV_BUFFER_SIZE);
    if (!conn->rx_buf) {
        INFRA_LOG_ERROR("Failed to allocate receive buffer");
        if (conn->store) {
            poly_db_close(conn->store);
        }
        infra_free(conn);
        infra_net_close(client);
        return;
    }
    conn->rx_len = 0;

    // Store connection context
    args->user_data = conn;
    conn->is_initialized = true;

    INFRA_LOG_INFO("Client connection initialized successfully: %s", conn->client_addr);
}

static void handle_request(void* args) {
    if (!args) {
        INFRA_LOG_ERROR("NULL handler args");
        return;
    }
    
    poly_poll_handler_args_t* handler_args = (poly_poll_handler_args_t*)args;
    if (!handler_args->client || handler_args->client < 0) {
        INFRA_LOG_ERROR("Invalid client socket: %d", handler_args->client);
        return;
    }
    
    memkv_conn_t* conn = (memkv_conn_t*)handler_args->user_data;
    if (!conn) {
        // 首次调用,创建新连接
        handle_connection(handler_args);
        if (!handler_args->user_data) {
            INFRA_LOG_ERROR("Failed to create connection context");
            return;
        }
        conn = (memkv_conn_t*)handler_args->user_data;
        INFRA_LOG_DEBUG("New connection created and initialized");
        return;  // 返回等待下一次调用
    }
    
    if (!conn->is_initialized || conn->is_closing) {
        INFRA_LOG_ERROR("Invalid connection state");
        if (conn) {
            memkv_conn_destroy(conn);
            handler_args->user_data = NULL;
        }
        return;
    }

    // 确保接收缓冲区有效
    if (!conn->rx_buf) {
        INFRA_LOG_ERROR("Invalid receive buffer for %s", conn->client_addr);
        goto cleanup;
    }
    
    // 确保socket有效
    if (conn->sock <= 0) {
        INFRA_LOG_ERROR("Invalid socket for %s", conn->client_addr);
        goto cleanup;
    }
    
    // 接收数据
    size_t received = 0;
    memset(conn->rx_buf, 0, MEMKV_BUFFER_SIZE);
    
    infra_error_t err = infra_net_recv(conn->sock, conn->rx_buf, MEMKV_BUFFER_SIZE - 1, &received);
    if (err != INFRA_OK) {
        if (err == INFRA_ERROR_WOULD_BLOCK) {
            return;  // 非阻塞模式下没有数据可读,等待下次调用
        }
        if (err == INFRA_ERROR_TIMEOUT) {
            return;  // 读取超时,等待下次调用
        }
        INFRA_LOG_ERROR("Failed to receive data from %s: %d", conn->client_addr, err);
        goto cleanup;
    }
    
    if (received == 0) {
        INFRA_LOG_INFO("Client %s disconnected", conn->client_addr);
        goto cleanup;
    }

    conn->rx_buf[received] = '\0';  // Ensure null termination
    conn->last_active_time = time(NULL);

    // Parse command
    char cmd[32] = {0};
    char key[256] = {0};
    
    // 检查命令格式
    if (sscanf(conn->rx_buf, "%31s %255s", cmd, key) < 1) {
        INFRA_LOG_ERROR("Failed to parse command from %s: [%s]", conn->client_addr, conn->rx_buf);
        conn->failed_commands++;
        if (conn->sock > 0) {
            err = send_all(conn->sock, "ERROR\r\n", 7);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send error response: %d", err);
                conn->should_close = true;
            }
        }
        return;  // 继续等待下一个命令
    }

    INFRA_LOG_DEBUG("Received command from %s: %s, key: %s", conn->client_addr, cmd, key);
    conn->total_commands++;
    
    // 处理命令
    if (strcmp(cmd, "get") == 0) {
        handle_get(conn, key);
    }
    else if (strcmp(cmd, "set") == 0) {
        char flags_str[32] = {0};
        char exptime_str[32] = {0};
        char bytes_str[32] = {0};
        
        if (sscanf(conn->rx_buf, "%*s %*s %31s %31s %31s", 
            flags_str, exptime_str, bytes_str) == 3) {
            handle_set(conn, key, flags_str, exptime_str, bytes_str, false);
        } else {
            INFRA_LOG_ERROR("Invalid SET command format from %s: [%s]", conn->client_addr, conn->rx_buf);
            conn->failed_commands++;
            if (conn->sock > 0) {
                err = send_all(conn->sock, "CLIENT_ERROR bad command line format\r\n", 37);
                if (err != INFRA_OK) {
                    INFRA_LOG_ERROR("Failed to send error response: %d", err);
                    conn->should_close = true;
                }
            }
        }
    }
    else if (strcmp(cmd, "delete") == 0) {
        handle_delete(conn, key, false);
    }
    else if (strcmp(cmd, "flush_all") == 0) {
        handle_flush(conn, false);
    }
    else if (strcmp(cmd, "incr") == 0 || strcmp(cmd, "decr") == 0) {
        char value_str[32] = {0};
        if (sscanf(conn->rx_buf, "%*s %*s %31s", value_str) == 1) {
            handle_incr_decr(conn, key, value_str, cmd[0] == 'i');
        } else {
            INFRA_LOG_ERROR("Invalid INCR/DECR command format from %s: [%s]", conn->client_addr, conn->rx_buf);
            conn->failed_commands++;
            if (conn->sock > 0) {
                err = send_all(conn->sock, "CLIENT_ERROR bad command line format\r\n", 37);
                if (err != INFRA_OK) {
                    INFRA_LOG_ERROR("Failed to send error response: %d", err);
                    conn->should_close = true;
                }
            }
        }
    }
    else if (strcmp(cmd, "quit") == 0) {
        INFRA_LOG_INFO("Client %s requested quit", conn->client_addr);
        goto cleanup;
    }
    else {
        INFRA_LOG_ERROR("Unknown command from %s: %s", conn->client_addr, cmd);
        conn->failed_commands++;
        if (conn->sock > 0) {
            err = send_all(conn->sock, "ERROR\r\n", 7);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send error response: %d", err);
                conn->should_close = true;
            }
        }
    }

    // 检查连接状态
    if (conn->should_close) {
        INFRA_LOG_INFO("Connection marked for closing from %s", conn->client_addr);
        goto cleanup;
    }
    
    return;  // 继续等待下一个命令

cleanup:
    INFRA_LOG_INFO("Closing connection from %s", conn->client_addr);
    if (conn) {
        memkv_conn_destroy(conn);
        handler_args->user_data = NULL;
    }
    INFRA_LOG_DEBUG("Connection cleanup completed for %s", conn->client_addr);
}

static void handle_request_wrapper(void* args) {
    if (!args) {
        INFRA_LOG_ERROR("NULL handler args");
        return;
    }
    
    poly_poll_handler_args_t* handler_args = (poly_poll_handler_args_t*)args;
    if (!handler_args->client || handler_args->client < 0) {
        INFRA_LOG_ERROR("Invalid client socket: %d", handler_args->client);
        return;
    }
    
    handle_request(args);
}

//-----------------------------------------------------------------------------
// Service Interface Implementation
//-----------------------------------------------------------------------------

infra_error_t memkv_init(void) {
    if (g_memkv_service.state != PEER_SERVICE_STATE_INIT &&
        g_memkv_service.state != PEER_SERVICE_STATE_STOPPED) {
        return INFRA_ERROR_INVALID_STATE;
    }
    
    g_memkv_state.port = MEMKV_DEFAULT_PORT;
    g_memkv_state.engine = strdup("sqlite");
    g_memkv_state.running = false;
    g_memkv_state.ctx = NULL;

    g_memkv_service.state = PEER_SERVICE_STATE_READY;
    return INFRA_OK;
}

infra_error_t memkv_cleanup(void) {
    if (g_memkv_service.state == PEER_SERVICE_STATE_RUNNING) {
        return INFRA_ERROR_INVALID_STATE;
    }

    if (g_memkv_state.running) {
        memkv_stop();
    }

    if (g_memkv_state.engine) {
        free(g_memkv_state.engine);
        g_memkv_state.engine = NULL;
    }
    if (g_memkv_state.plugin) {
        free(g_memkv_state.plugin);
        g_memkv_state.plugin = NULL;
    }

    g_memkv_service.state = PEER_SERVICE_STATE_INIT;
    return INFRA_OK;
}

infra_error_t memkv_start(void) {
    infra_error_t err;
    
    if (g_memkv_service.state == PEER_SERVICE_STATE_INIT) {
        err = memkv_init();
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to initialize service: %d", err);
            return err;
        }
    }

    if (g_memkv_service.state != PEER_SERVICE_STATE_READY &&
        g_memkv_service.state != PEER_SERVICE_STATE_STOPPED) {
        INFRA_LOG_ERROR("Invalid service state: %d", g_memkv_service.state);
        return INFRA_ERROR_INVALID_STATE;
    }

    if (g_memkv_state.running) {
        INFRA_LOG_ERROR("Service is already running");
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    // Create poll context
    g_memkv_state.ctx = (poly_poll_context_t*)infra_malloc(sizeof(poly_poll_context_t));
    if (!g_memkv_state.ctx) {
        INFRA_LOG_ERROR("Failed to allocate poll context");
        return INFRA_ERROR_NO_MEMORY;
    }
    memset(g_memkv_state.ctx, 0, sizeof(poly_poll_context_t));

    // Initialize poll context
    poly_poll_config_t config = {
        .min_threads = 2,
        .max_threads = MEMKV_MAX_THREADS,
        .queue_size = 1000,
        .max_listeners = 1,
        .read_buffer_size = MEMKV_BUFFER_SIZE
    };

    err = poly_poll_init(g_memkv_state.ctx, &config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize poll context: %d", err);
        infra_free(g_memkv_state.ctx);
        g_memkv_state.ctx = NULL;
        return err;
    }

    // Set request handler
    poly_poll_set_handler(g_memkv_state.ctx, handle_request_wrapper);

    // Add listener
    poly_poll_listener_t listener = {0};
    listener.bind_port = g_memkv_state.port;
    strncpy(listener.bind_addr, "0.0.0.0", sizeof(listener.bind_addr) - 1);
    listener.bind_addr[sizeof(listener.bind_addr) - 1] = '\0';

    err = poly_poll_add_listener(g_memkv_state.ctx, &listener);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to add listener: %d", err);
        poly_poll_cleanup(g_memkv_state.ctx);
        infra_free(g_memkv_state.ctx);
        g_memkv_state.ctx = NULL;
        return err;
    }

    // Start polling
    err = poly_poll_start(g_memkv_state.ctx);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to start polling: %d", err);
        poly_poll_cleanup(g_memkv_state.ctx);
        infra_free(g_memkv_state.ctx);
        g_memkv_state.ctx = NULL;
        return err;
    }

    g_memkv_state.running = true;
    g_memkv_service.state = PEER_SERVICE_STATE_RUNNING;
    
    INFRA_LOG_INFO("MemKV service started successfully on port %d", g_memkv_state.port);
    return INFRA_OK;
}

infra_error_t memkv_stop(void) {
    if (g_memkv_service.state != PEER_SERVICE_STATE_RUNNING) {
        return INFRA_ERROR_INVALID_STATE;
    }

    if (!g_memkv_state.running) {
        return INFRA_OK;
    }

    g_memkv_state.running = false;

    if (g_memkv_state.ctx) {
        poly_poll_stop(g_memkv_state.ctx);
        poly_poll_cleanup(g_memkv_state.ctx);
        infra_free(g_memkv_state.ctx);
        g_memkv_state.ctx = NULL;
    }

    g_memkv_service.state = PEER_SERVICE_STATE_STOPPED;
    return INFRA_OK;
}

infra_error_t memkv_cmd_handler(const char* cmd, char* response, size_t size) {
    if (!cmd || !response || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    char cmd_copy[256];
    strncpy(cmd_copy, cmd, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';

    char* argv[16];
    int argc = 0;
    char* token = strtok(cmd_copy, " ");
    while (token && argc < 16) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }

    if (argc == 0) {
        snprintf(response, size, "Empty command");
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (strcmp(argv[0], "start") == 0) {
        infra_error_t err = memkv_start();
        snprintf(response, size, err == INFRA_OK ?
                "MemKV service started\n" : "Failed to start MemKV service: %d\n", err);
        return err;
    }
    else if (strcmp(argv[0], "stop") == 0) {
        infra_error_t err = memkv_stop();
        snprintf(response, size, err == INFRA_OK ?
                "MemKV service stopped\n" : "Failed to stop MemKV service: %d\n", err);
        return err;
    }

    snprintf(response, size, "Unknown command: %s", argv[0]);
    return INFRA_ERROR_NOT_FOUND;
}

// Apply configuration
infra_error_t memkv_apply_config(const poly_service_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (g_memkv_service.state != PEER_SERVICE_STATE_READY &&
        g_memkv_service.state != PEER_SERVICE_STATE_STOPPED) {
        INFRA_LOG_ERROR("Service is in invalid state: %d", g_memkv_service.state);
        return INFRA_ERROR_INVALID_STATE;
    }

    // 直接使用配置中的端口
    g_memkv_state.port = config->listen_port;
    
    // 保存旧的 engine 指针
    char* old_engine = g_memkv_state.engine;
    
    // 设置新的 engine
    if (config->backend && config->backend[0]) {
        g_memkv_state.engine = strdup(config->backend);
        if (!g_memkv_state.engine) {
            INFRA_LOG_ERROR("Failed to allocate memory for engine");
            return INFRA_ERROR_NO_MEMORY;
        }
    } else {
        g_memkv_state.engine = strdup("sqlite");  // 默认使用 sqlite
        if (!g_memkv_state.engine) {
            INFRA_LOG_ERROR("Failed to allocate memory for default engine");
            return INFRA_ERROR_NO_MEMORY;
        }
    }
    
    // 最后释放旧的 engine
    if (old_engine) {
        free(old_engine);
    }

    INFRA_LOG_INFO("Applied configuration - port: %d, engine: %s",
        g_memkv_state.port,
        g_memkv_state.engine ? g_memkv_state.engine : "default");

    return INFRA_OK;
}

peer_service_t* peer_memkv_get_service(void) {
    return &g_memkv_service;
}

static void memkv_conn_destroy(memkv_conn_t* conn) {
    if (!conn) {
        INFRA_LOG_ERROR("Attempting to destroy NULL connection");
        return;
    }

    if (conn->is_closing) {
        INFRA_LOG_DEBUG("Connection already being destroyed");
        return;
    }
    conn->is_closing = true;

    INFRA_LOG_INFO("Destroying connection from %s (commands: total=%zu, failed=%zu)", 
                   conn->client_addr, conn->total_commands, conn->failed_commands);

    // Close database connection
    if (conn->store) {
        INFRA_LOG_DEBUG("Closing database connection");
        poly_db_exec(conn->store, "PRAGMA optimize;");  // 优化数据库
        poly_db_close(conn->store);
        conn->store = NULL;
    }

    // Free receive buffer
    if (conn->rx_buf) {
        INFRA_LOG_DEBUG("Freeing receive buffer");
        infra_free(conn->rx_buf);
        conn->rx_buf = NULL;
    }

    // Close socket
    if (conn->sock > 0) {
        INFRA_LOG_DEBUG("Closing client socket");
        infra_net_close(conn->sock);
        conn->sock = -1;
    }

    // Calculate connection lifetime
    uint64_t end_time = time(NULL);
    uint64_t lifetime = end_time - conn->created_time;
    uint64_t idle_time = end_time - conn->last_active_time;

    INFRA_LOG_INFO("Connection statistics - lifetime: %lus, idle: %lus, commands: %zu, failed: %zu",
                   lifetime, idle_time, conn->total_commands, conn->failed_commands);

    INFRA_LOG_DEBUG("Freeing connection structure");
    infra_free(conn);
}

static memkv_conn_t* memkv_conn_create(infra_socket_t client) {
    memkv_state_t* state = get_state();
    if (!state) {
        INFRA_LOG_ERROR("Service state not initialized");
        return NULL;
    }

    memkv_conn_t* conn = (memkv_conn_t*)infra_malloc(sizeof(memkv_conn_t));
    if (!conn) {
        INFRA_LOG_ERROR("Failed to allocate connection");
        return NULL;
    }
    
    conn->sock = client;
    conn->store = NULL;
    conn->rx_buf = NULL;
    conn->rx_len = 0;
    conn->should_close = false;
    conn->is_closing = false;
    conn->is_initialized = false;
    conn->created_time = time(NULL);
    conn->last_active_time = conn->created_time;
    conn->total_commands = 0;
    conn->failed_commands = 0;
    
    // 设置 socket 为非阻塞模式
    infra_error_t err = infra_net_set_nonblock(client, true);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set socket to non-blocking mode");
        infra_free(conn);
        return NULL;
    }

    // 设置 TCP keepalive
    err = infra_net_set_keepalive(client, true);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set TCP keepalive");
        infra_free(conn);
        return NULL;
    }

    // 设置 TCP keepalive 参数
    err = infra_net_set_keepalive_params(client, 60, 10, 6);
    if (err != INFRA_OK) {
        INFRA_LOG_WARN("Failed to set TCP keepalive parameters (this is not fatal)");
    }

    // Open database connection using poly_db with shared cache
    poly_db_config_t config = {
        .type = POLY_DB_TYPE_SQLITE,
        .url = state->db_path,
        .max_memory = 100 * 1024 * 1024,  // 100MB 内存限制
        .read_only = false,
        .plugin_path = NULL,
        .allow_fallback = false
    };
    
    INFRA_LOG_INFO("Opening database: %s", state->db_path);
    
    err = poly_db_open(&config, &conn->store);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to open database connection: %d", err);
        infra_free(conn);
        return NULL;
    }

    // 启用 WAL 模式以提高并发性能
    err = poly_db_exec(conn->store, "PRAGMA journal_mode=WAL;");
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to enable WAL mode");
        poly_db_close(conn->store);
        infra_free(conn);
        return NULL;
    }

    // 设置较短的超时和重试
    err = poly_db_exec(conn->store, "PRAGMA busy_timeout=5000;");
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set busy timeout");
        poly_db_close(conn->store);
        infra_free(conn);
        return NULL;
    }

    // 设置共享缓存模式
    err = poly_db_exec(conn->store, "PRAGMA cache_size=2000;");  // 2000 pages = ~8MB cache
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set cache size");
        poly_db_close(conn->store);
        infra_free(conn);
        return NULL;
    }

    err = poly_db_exec(conn->store, "PRAGMA synchronous=NORMAL;");  // 提高写入性能
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set synchronous mode");
        poly_db_close(conn->store);
        infra_free(conn);
        return NULL;
    }

    err = poly_db_exec(conn->store, "PRAGMA locking_mode=NORMAL;");  // 使用正常的锁定模式
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set locking mode");
        poly_db_close(conn->store);
        infra_free(conn);
        return NULL;
    }
    
    INFRA_LOG_INFO("Database connection established");
    return conn;
}
