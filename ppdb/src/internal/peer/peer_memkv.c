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
#include <ctype.h>  // 添加 ctype.h 头文件

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
static void handle_request(void* args);
static void handle_connection(poly_poll_handler_args_t* args);
static int handle_get(memkv_conn_t* conn, const char* key);
static void handle_delete(memkv_conn_t* conn, const char* key, bool noreply);
static void handle_flush(memkv_conn_t* conn, bool noreply);
static void handle_incr_decr(memkv_conn_t* conn, const char* key, const char* value_str, bool is_incr);

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

// 获取服务状态的辅助函数
static inline memkv_state_t* get_state(void) {
    return (memkv_state_t*)g_memkv_service.config.user_data;
}

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

static infra_error_t db_init(poly_db_t** db) {
    if (!db) return INFRA_ERROR_INVALID_PARAM;
    
    memkv_state_t* state = get_state();
    if (!state) {
        INFRA_LOG_ERROR("Service state not initialized");
        return INFRA_ERROR_INVALID_STATE;
    }
    
    poly_db_config_t config = {
        .type = state->engine && strcmp(state->engine, "duckdb") == 0 ? 
                POLY_DB_TYPE_DUCKDB : POLY_DB_TYPE_SQLITE,
        .url = state->plugin ? state->plugin : ":memory:",
        .max_memory = 0,
        .read_only = false,
        .plugin_path = state->plugin,
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
                           size_t* value_len, uint32_t* flags, uint32_t* exptime) {
    if (!db || !key || !value || !value_len || !flags || !exptime) {
        printf("DEBUG: Invalid parameters in kv_get\n");
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    printf("DEBUG: kv_get for key: [%s]\n", key);
    
    const char* sql = 
        "SELECT value, flags, expiry FROM kv_store WHERE key = ?";
    
    poly_db_stmt_t* stmt = NULL;
    infra_error_t err = poly_db_prepare(db, sql, &stmt);
    if (err != INFRA_OK) {
        printf("DEBUG: Failed to prepare statement: %d\n", err);
        return err;
    }
    
    err = poly_db_bind_text(stmt, 1, key, strlen(key));
    if (err != INFRA_OK) {
        printf("DEBUG: Failed to bind key: %d\n", err);
        poly_db_stmt_finalize(stmt);
        return err;
    }
    
    err = poly_db_stmt_step(stmt);
    if (err == INFRA_ERROR_NOT_FOUND) {
        printf("DEBUG: Key not found: [%s]\n", key);
        poly_db_stmt_finalize(stmt);
        return INFRA_ERROR_NOT_FOUND;
    }
    if (err != INFRA_OK) {
        printf("DEBUG: Failed to execute statement: %d\n", err);
        poly_db_stmt_finalize(stmt);
        return err;
    }
    
    // 获取 BLOB 数据
    void* blob_data = NULL;
    size_t blob_size = 0;
    err = poly_db_column_blob(stmt, 0, &blob_data, &blob_size);
    if (err != INFRA_OK || !blob_data || blob_size == 0) {
        printf("DEBUG: Failed to get blob data: err=%d, data=%p, size=%zu\n", 
               err, blob_data, blob_size);
        poly_db_stmt_finalize(stmt);
        return INFRA_ERROR_NOT_FOUND;
    }
    
    printf("DEBUG: Got blob data: size=%zu\n", blob_size);
    
    // 分配内存并复制数据
    void* data = malloc(blob_size);
    if (!data) {
        printf("DEBUG: Failed to allocate memory for blob data\n");
        poly_db_stmt_finalize(stmt);
        return INFRA_ERROR_NO_MEMORY;
    }
    
    memcpy(data, blob_data, blob_size);
    *value = data;
    *value_len = blob_size;
    
    // 获取 flags
    char* flags_str = NULL;
    err = poly_db_column_text(stmt, 1, &flags_str);
    if (err == INFRA_OK && flags_str) {
        *flags = (uint32_t)strtoul(flags_str, NULL, 10);
        free(flags_str);
    } else {
        printf("DEBUG: Failed to get flags: err=%d\n", err);
        *flags = 0;
    }
    
    // 获取 exptime
    char* exptime_str = NULL;
    err = poly_db_column_text(stmt, 2, &exptime_str);
    if (err == INFRA_OK && exptime_str) {
        *exptime = (uint32_t)strtoul(exptime_str, NULL, 10);
        free(exptime_str);
    } else {
        printf("DEBUG: Failed to get exptime: err=%d\n", err);
        *exptime = 0;
    }
    
    printf("DEBUG: Successfully got key-value pair: [%s]=[%.*s], flags=%u, exptime=%u\n",
           key, (int)blob_size, (char*)data, *flags, *exptime);
    
    poly_db_stmt_finalize(stmt);
    return INFRA_OK;
}

static infra_error_t kv_set(poly_db_t* db, const char* key, const void* value, 
                           size_t value_len, uint32_t flags, uint32_t exptime) {
    if (!db || !key || !value || value_len == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    const char* sql = 
        "INSERT OR REPLACE INTO kv_store (key, value, flags, expiry) "
        "VALUES (?, ?, ?, ?)";
    
    poly_db_stmt_t* stmt = NULL;
    infra_error_t err = poly_db_prepare(db, sql, &stmt);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to prepare statement: %d", err);
        return err;
    }
    
    err = poly_db_bind_text(stmt, 1, key, strlen(key));
    if (err != INFRA_OK) goto cleanup;
    
    err = poly_db_bind_blob(stmt, 2, value, value_len);
    if (err != INFRA_OK) goto cleanup;
    
    char flags_str[32];
    snprintf(flags_str, sizeof(flags_str), "%u", flags);
    err = poly_db_bind_text(stmt, 3, flags_str, strlen(flags_str));
    if (err != INFRA_OK) goto cleanup;
    
    char exptime_str[32];
    snprintf(exptime_str, sizeof(exptime_str), "%u", exptime);
    err = poly_db_bind_text(stmt, 4, exptime_str, strlen(exptime_str));
    if (err != INFRA_OK) goto cleanup;
    
    err = poly_db_stmt_step(stmt);
    
cleanup:
    poly_db_stmt_finalize(stmt);
    return err;
}

static infra_error_t kv_delete(poly_db_t* db, const char* key) {
    infra_error_t err;
    
    // 开始事务
    err = poly_db_exec(db, "BEGIN TRANSACTION");
    if (err != INFRA_OK) return err;
    
    // 先检查键是否存在
    const char* check_sql = "SELECT 1 FROM kv_store WHERE key = ?";
    poly_db_stmt_t* check_stmt = NULL;
    err = poly_db_prepare(db, check_sql, &check_stmt);
    if (err != INFRA_OK) {
        poly_db_exec(db, "ROLLBACK");
        return err;
    }
    
    err = poly_db_bind_text(check_stmt, 1, key, strlen(key));
    if (err != INFRA_OK) {
        poly_db_stmt_finalize(check_stmt);
        poly_db_exec(db, "ROLLBACK");
        return err;
    }
    
    err = poly_db_stmt_step(check_stmt);
    bool exists = (err == INFRA_OK);  // 如果找到记录，err 会是 INFRA_OK
    poly_db_stmt_finalize(check_stmt);
    
    if (!exists) {
        poly_db_exec(db, "ROLLBACK");
        return INFRA_ERROR_NOT_FOUND;
    }
    
    // 键存在，执行删除
    const char* delete_sql = "DELETE FROM kv_store WHERE key = ?";
    poly_db_stmt_t* delete_stmt = NULL;
    
    err = poly_db_prepare(db, delete_sql, &delete_stmt);
    if (err != INFRA_OK) {
        poly_db_exec(db, "ROLLBACK");
        return err;
    }
    
    err = poly_db_bind_text(delete_stmt, 1, key, strlen(key));
    if (err != INFRA_OK) {
        poly_db_stmt_finalize(delete_stmt);
        poly_db_exec(db, "ROLLBACK");
        return err;
    }
    
    err = poly_db_stmt_step(delete_stmt);
    poly_db_stmt_finalize(delete_stmt);
    
    if (err != INFRA_OK) {
        poly_db_exec(db, "ROLLBACK");
        return err;
    }
    
    // 提交事务
    return poly_db_exec(db, "COMMIT");
}

static infra_error_t kv_flush(poly_db_t* db) {
    return poly_db_exec(db, "DELETE FROM kv_store");
}

static int send_all(infra_socket_t sock, const void* data, size_t len) {
    if (sock <= 0 || !data || len == 0) {
        printf("DEBUG: Invalid parameters in send_all: sock=%d, data=%p, len=%zu\n", 
               sock, data, len);
        return INFRA_ERROR_INVALID_PARAM;
    }

    const char* buf = (const char*)data;
    size_t sent = 0;
    int retry_count = 0;
    const int max_retries = 3;

    printf("DEBUG: Starting to send %zu bytes\n", len);

    while (sent < len) {
        size_t bytes_sent = 0;
        infra_error_t err = infra_net_send(sock, (const char*)data + sent, len - sent, &bytes_sent);
        
        printf("DEBUG: Send attempt %d: tried to send %zu bytes, sent %zu bytes, err=%d\n",
               retry_count + 1, len - sent, bytes_sent, err);
        
        if (err == INFRA_ERROR_WOULD_BLOCK) {
            if (retry_count < max_retries) {
                printf("DEBUG: Would block, retrying after 10ms (retry %d/%d)\n",
                       retry_count + 1, max_retries);
                usleep(10000); // 10ms
                retry_count++;
                continue;
            }
            printf("DEBUG: Send would block after %d retries\n", max_retries);
            return err;
        }
        
        if (err != INFRA_OK) {
            printf("DEBUG: Failed to send data: err=%d\n", err);
            return err;
        }
        
        if (bytes_sent == 0) {
            printf("DEBUG: Connection closed by peer\n");
            return INFRA_ERROR_CLOSED;
        }
        
        sent += bytes_sent;
        retry_count = 0; // 重置重试计数
        
        printf("DEBUG: Successfully sent %zu/%zu bytes\n", sent, len);
    }

    printf("DEBUG: Successfully sent all %zu bytes\n", len);
    return INFRA_OK;
}

static int handle_get(memkv_conn_t* conn, const char* key) {
    if (!conn || !key) {
        INFRA_LOG_ERROR("Invalid parameters in handle_get");
        return -1;
    }
    
    INFRA_LOG_INFO("handle_get for key: [%s]", key);
    
    void* value = NULL;
    size_t value_len = 0;
    uint32_t flags = 0;
    uint32_t exptime = 0;
    
    infra_error_t err = kv_get(conn->store, key, &value, &value_len, &flags, &exptime);
    if (err == INFRA_ERROR_NOT_FOUND) {
        INFRA_LOG_INFO("Key not found: [%s]", key);
        err = send_all(conn->sock, "END\r\n", 5);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to send END marker: %d", err);
            return -1;
        }
        return 0;  // 键不存在，返回0表示处理完成但未找到键
    }
    if (err != INFRA_OK || !value) {
        INFRA_LOG_ERROR("Failed to get key: [%s], err=%d", key, err);
        return -1;
    }
    
    INFRA_LOG_INFO("Found key: [%s], value_len=%zu, flags=%u, exptime=%u", 
                   key, value_len, flags, exptime);
    
    // 构造响应头
    char header[256];
    int header_len = snprintf(header, sizeof(header), "VALUE %s %u %zu\r\n", 
                            key, flags, value_len);
    if (header_len < 0 || (size_t)header_len >= sizeof(header)) {
        INFRA_LOG_ERROR("Response header buffer overflow");
        free(value);
        return -1;
    }
    
    INFRA_LOG_INFO("Sending response header: [%.*s]", header_len, header);
    
    // 发送响应头
    err = send_all(conn->sock, header, header_len);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to send response header: err=%d", err);
        free(value);
        return -1;
    }
    
    // 发送值
    INFRA_LOG_INFO("Sending value: [%.*s]", (int)value_len, (char*)value);
    err = send_all(conn->sock, value, value_len);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to send value: err=%d", err);
        free(value);
        return -1;
    }
    
    // 发送值后的换行
    err = send_all(conn->sock, "\r\n", 2);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to send value terminator: err=%d", err);
        free(value);
        return -1;
    }
    
    // 发送 END 标记
    err = send_all(conn->sock, "END\r\n", 5);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to send END marker: err=%d", err);
        free(value);
        return -1;
    }
    
    INFRA_LOG_INFO("Successfully sent key-value pair: [%s]=[%.*s]", 
                   key, (int)value_len, (char*)value);
    
    free(value);
    return 1;  // 返回1表示成功找到并发送了键值对
}

static void handle_set(memkv_conn_t* conn, const char* key, const char* flags_str,
                      const char* exptime_str, const char* bytes_str, bool noreply,
                      const char* data_ptr) {
    if (!conn || !key || !flags_str || !exptime_str || !bytes_str || !data_ptr) {
        INFRA_LOG_ERROR("Invalid parameters in handle_set");
        if (!noreply && conn && conn->sock > 0) {
            infra_error_t err = send_all(conn->sock, "CLIENT_ERROR bad command line format\r\n", 37);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send error response: %d", err);
                conn->should_close = true;
            }
        }
        return;
    }

    INFRA_LOG_DEBUG("Handling SET command: key='%s', flags='%s', exptime='%s', bytes='%s'",
                    key, flags_str, exptime_str, bytes_str);

    // Parse parameters
    char* endptr;
    uint32_t flags = strtoul(flags_str, &endptr, 10);
    if (*endptr != '\0') {
        INFRA_LOG_ERROR("Invalid flags value: %s", flags_str);
        if (!noreply) {
            infra_error_t err = send_all(conn->sock, "CLIENT_ERROR invalid flags\r\n", 25);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send error response: %d", err);
                conn->should_close = true;
            }
        }
        return;
    }

    uint32_t exptime = strtoul(exptime_str, &endptr, 10);
    if (*endptr != '\0') {
        INFRA_LOG_ERROR("Invalid exptime value: %s", exptime_str);
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
    if (*endptr != '\0') {
        INFRA_LOG_ERROR("Invalid bytes value: %s", bytes_str);
        if (!noreply) {
            infra_error_t err = send_all(conn->sock, "CLIENT_ERROR invalid bytes\r\n", 25);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send error response: %d", err);
                conn->should_close = true;
            }
        }
        return;
    }

    INFRA_LOG_DEBUG("Parsed SET params: flags=%u, exptime=%u, bytes=%zu",
                    flags, exptime, bytes);

    // 检查数据长度
    size_t data_len = strlen(data_ptr);
    if (data_len != bytes) {
        INFRA_LOG_ERROR("Data length mismatch: expected %zu, got %zu", bytes, data_len);
        if (!noreply) {
            infra_error_t err = send_all(conn->sock, "CLIENT_ERROR length mismatch\r\n", 29);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send error response: %d", err);
                conn->should_close = true;
            }
        }
        return;
    }

    // 存储数据
    infra_error_t err = kv_set(conn->store, key, data_ptr, bytes, flags, exptime);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to store data: %d", err);
        if (!noreply) {
            err = send_all(conn->sock, "SERVER_ERROR\r\n", 14);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send error response: %d", err);
                conn->should_close = true;
            }
        }
        return;
    }

    // 发送响应
    if (!noreply) {
        err = send_all(conn->sock, "STORED\r\n", 8);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to send STORED response: %d", err);
            conn->should_close = true;
        }
    }
}

static void handle_delete(memkv_conn_t* conn, const char* key, bool noreply) {
    printf("DEBUG: Handling DELETE command for key='%s'\n", key);
    
    infra_error_t err = kv_delete(conn->store, key);
    if (err == INFRA_OK) {
        if (!noreply) {
            err = send_all(conn->sock, "DELETED\r\n", 9);
            if (err != INFRA_OK) {
                printf("DEBUG: Failed to send DELETED response: err=%d\n", err);
                conn->should_close = true;
            }
        }
    } else if (err == INFRA_ERROR_NOT_FOUND) {
        if (!noreply) {
            err = send_all(conn->sock, "NOT_FOUND\r\n", 11);
            if (err != INFRA_OK) {
                printf("DEBUG: Failed to send NOT_FOUND response: err=%d\n", err);
                conn->should_close = true;
            }
        }
    } else {
        if (!noreply) {
            err = send_all(conn->sock, "SERVER_ERROR\r\n", 14);
            if (err != INFRA_OK) {
                printf("DEBUG: Failed to send SERVER_ERROR response: err=%d\n", err);
                conn->should_close = true;
            }
        }
    }
}

static void handle_flush(memkv_conn_t* conn, bool noreply) {
    infra_error_t err = poly_db_exec(conn->store, "DELETE FROM kv_store");
    if (!noreply) {
        if (err == INFRA_OK) {
            infra_net_send(conn->sock, "OK\r\n", 4, NULL);
        } else {
            infra_net_send(conn->sock, "ERROR\r\n", 7, NULL);
        }
    }
}

static void handle_incr_decr(memkv_conn_t* conn, const char* key, const char* value_str, bool is_incr) {
    printf("DEBUG: Handling %s command for key='%s', value='%s'\n", 
           is_incr ? "INCR" : "DECR", key, value_str);
           
    uint64_t delta = strtoull(value_str, NULL, 10);
    void* old_value = NULL;
    size_t old_value_len = 0;
    uint32_t flags = 0;
    uint32_t exptime = 0;
    
    infra_error_t err = kv_get(conn->store, key, &old_value, &old_value_len, &flags, &exptime);
    if (err != INFRA_OK || !old_value) {
        if (is_incr) {
            // 对于INCR，如果key不存在，初始化为0
            char zero_str[] = "0";
            err = kv_set(conn->store, key, zero_str, strlen(zero_str), 0, 0);
            if (err == INFRA_OK) {
                err = send_all(conn->sock, "0\r\n", 3);
                if (err == INFRA_ERROR_CLOSED) {
                    conn->should_close = true;
                }
            } else {
                err = send_all(conn->sock, "ERROR\r\n", 7);
                if (err == INFRA_ERROR_CLOSED) {
                    conn->should_close = true;
                }
            }
        } else {
            err = send_all(conn->sock, "NOT_FOUND\r\n", 11);
            if (err == INFRA_ERROR_CLOSED) {
                conn->should_close = true;
            }
        }
        if (old_value) free(old_value);
        return;
    }
    
    // 确保old_value是以null结尾的字符串
    char* null_term_value = malloc(old_value_len + 1);
    if (!null_term_value) {
        free(old_value);
        err = send_all(conn->sock, "SERVER_ERROR out of memory\r\n", 26);
        if (err == INFRA_ERROR_CLOSED) {
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
    
    err = kv_set(conn->store, key, new_value, new_value_len, flags, 0);
    if (err == INFRA_OK) {
        char response[32];
        int response_len = snprintf(response, sizeof(response), "%lu\r\n", current);
        err = send_all(conn->sock, response, response_len);
        if (err == INFRA_ERROR_CLOSED) {
            conn->should_close = true;
        }
    } else {
        err = send_all(conn->sock, "ERROR\r\n", 7);
        if (err == INFRA_ERROR_CLOSED) {
            conn->should_close = true;
        }
    }
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
        INFRA_LOG_DEBUG("Closing socket");
        infra_net_close(conn->sock);
        conn->sock = -1;
    }

    // Free connection state
    infra_free(conn);
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
        conn->is_initialized = true;
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

    INFRA_LOG_INFO("Received data from %s: [%s]", conn->client_addr, conn->rx_buf);

    // 处理每一行命令
    char* line = conn->rx_buf;
    char* next_line;
    bool in_set_data = false;
    char* set_key = NULL;
    char* set_flags = NULL;
    char* set_exptime = NULL;
    char* set_bytes = NULL;
    bool set_noreply = false;
    char* set_data = NULL;

    while (line && *line) {
        // 找到下一行
        next_line = strstr(line, "\r\n");
        if (next_line) {
            *next_line = '\0';
            next_line += 2;
        }

        // 跳过空行
        if (*line == '\0') {
            line = next_line;
            continue;
        }

        // 如果正在处理 SET 命令的数据部分
        if (in_set_data) {
            // 处理 SET 命令
            handle_set(conn, set_key, set_flags, set_exptime, set_bytes, set_noreply, line);
            
            // 清除 SET 命令状态
            free(set_key);
            free(set_flags);
            free(set_exptime);
            free(set_bytes);
            set_key = NULL;
            set_flags = NULL;
            set_exptime = NULL;
            set_bytes = NULL;
            set_noreply = false;
            in_set_data = false;
            line = next_line;
            continue;
        }
        
        // Parse command
        char cmd[32] = {0};
        char key[256] = {0};
        
        // 检查命令格式
        if (sscanf(line, "%31s %255s", cmd, key) < 1) {
            INFRA_LOG_ERROR("Failed to parse command from %s: [%s]", conn->client_addr, line);
            conn->failed_commands++;
            if (conn->sock > 0) {
                err = send_all(conn->sock, "ERROR\r\n", 7);
                if (err != INFRA_OK) {
                    INFRA_LOG_ERROR("Failed to send error response: %d", err);
                    conn->should_close = true;
                }
            }
            line = next_line;
            continue;
        }

        INFRA_LOG_INFO("Processing command from %s: %s, key: %s", conn->client_addr, cmd, key);
        conn->total_commands++;
        
        // 处理命令
        if (strcasecmp(cmd, "get") == 0) {
            handle_get(conn, key);
            err = send_all(conn->sock, "END\r\n", 5);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send END marker: %d", err);
                conn->should_close = true;
            }
        }
        else if (strcasecmp(cmd, "set") == 0) {
            char flags_str[32] = {0};
            char exptime_str[32] = {0};
            char bytes_str[32] = {0};
            char noreply_str[32] = {0};
            
            int matched = sscanf(line, "%*s %*s %31s %31s %31s %31s", 
                flags_str, exptime_str, bytes_str, noreply_str);
            if (matched >= 3) {
                // 保存 SET 命令参数
                set_key = strdup(key);
                set_flags = strdup(flags_str);
                set_exptime = strdup(exptime_str);
                set_bytes = strdup(bytes_str);
                set_noreply = (matched == 4 && strcmp(noreply_str, "noreply") == 0);
                in_set_data = true;  // 标记下一行为数据
            } else {
                INFRA_LOG_ERROR("Invalid SET command format from %s: [%s]", conn->client_addr, line);
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
        else if (strcasecmp(cmd, "delete") == 0) {
            char noreply_str[32] = {0};
            bool noreply = (sscanf(line, "%*s %*s %31s", noreply_str) == 1 && 
                          strcmp(noreply_str, "noreply") == 0);
            handle_delete(conn, key, noreply);
        }
        else if (strcasecmp(cmd, "flush_all") == 0) {
            char noreply_str[32] = {0};
            bool noreply = (sscanf(line, "%*s %*s %31s", noreply_str) == 1 && 
                          strcmp(noreply_str, "noreply") == 0);
            handle_flush(conn, noreply);
        }
        else if (strcasecmp(cmd, "incr") == 0 || strcasecmp(cmd, "decr") == 0) {
            char value_str[32] = {0};
            if (sscanf(line, "%*s %*s %31s", value_str) == 1) {
                handle_incr_decr(conn, key, value_str, cmd[0] == 'i');
            } else {
                INFRA_LOG_ERROR("Invalid INCR/DECR command format from %s: [%s]", 
                               conn->client_addr, line);
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
        else {
            // 如果不是已知命令，且不是 SET 数据，则报错
            if (!in_set_data) {
                INFRA_LOG_ERROR("Unknown command: [%s]", cmd);
                if (conn->sock > 0) {
                    err = send_all(conn->sock, "ERROR\r\n", 7);
                    if (err != INFRA_OK) {
                        INFRA_LOG_ERROR("Failed to send error response: %d", err);
                        conn->should_close = true;
                    }
                }
            }
        }
        
        line = next_line;
    }

    // 如果还在等待 SET 数据但没收到，报错
    if (in_set_data) {
        INFRA_LOG_ERROR("Missing data for SET command");
        conn->failed_commands++;
        if (!set_noreply && conn->sock > 0) {
            err = send_all(conn->sock, "CLIENT_ERROR missing data block\r\n", 31);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send error response: %d", err);
                conn->should_close = true;
            }
        }
        free(set_key);
        free(set_flags);
        free(set_exptime);
        free(set_bytes);
    }

    return;

cleanup:
    INFRA_LOG_INFO("Closing connection from %s", conn->client_addr);
    if (conn) {
        memkv_conn_destroy(conn);
        handler_args->user_data = NULL;
    }
    INFRA_LOG_DEBUG("Connection cleanup completed for %s", conn->client_addr);
}

static void handle_connection(poly_poll_handler_args_t* args) {
    if (!args) {
        INFRA_LOG_ERROR("Invalid handler args");
        return;
    }

    infra_socket_t client = args->client;
    if (client <= 0) {
        INFRA_LOG_ERROR("Invalid client socket");
        return;
    }

    // 创建连接状态
    memkv_conn_t* conn = (memkv_conn_t*)infra_malloc(sizeof(memkv_conn_t));
    if (!conn) {
        INFRA_LOG_ERROR("Failed to allocate connection state");
        infra_net_close(client);
        return;
    }
    memset(conn, 0, sizeof(memkv_conn_t));

    // 分配接收缓冲区
    conn->rx_buf = (char*)infra_malloc(MEMKV_CONN_BUFFER_SIZE);
    if (!conn->rx_buf) {
        INFRA_LOG_ERROR("Failed to allocate receive buffer");
        infra_free(conn);
        infra_net_close(client);
        return;
    }

    // 获取客户端地址
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getpeername(client, (struct sockaddr*)&addr, &addr_len) == 0) {
        snprintf(conn->client_addr, sizeof(conn->client_addr), "%s:%d",
                inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    } else {
        strncpy(conn->client_addr, "unknown", sizeof(conn->client_addr) - 1);
    }

    // 设置 TCP_NODELAY
    int flag = 1;
    if (setsockopt(client, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
        INFRA_LOG_ERROR("Failed to set TCP_NODELAY");
    }

    // 设置非阻塞模式
    infra_error_t err = infra_net_set_nonblock(client, true);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set non-blocking mode");
        infra_free(conn->rx_buf);
        infra_free(conn);
        infra_net_close(client);
        return;
    }

    // 设置 TCP keepalive
    flag = 1;
    if (setsockopt(client, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) < 0) {
        INFRA_LOG_ERROR("Failed to set SO_KEEPALIVE");
        infra_free(conn->rx_buf);
        infra_free(conn);
        infra_net_close(client);
        return;
    }

    // 设置 TCP keepalive 参数
#ifdef TCP_KEEPIDLE
    int keepalive_time = 60;  // 60 秒后开始发送 keepalive
    if (setsockopt(client, IPPROTO_TCP, TCP_KEEPIDLE, &keepalive_time, sizeof(keepalive_time)) < 0) {
        INFRA_LOG_WARN("Failed to set TCP_KEEPIDLE");
    }
#endif

#ifdef TCP_KEEPINTVL
    int keepalive_intvl = 10;  // 每 10 秒发送一次 keepalive
    if (setsockopt(client, IPPROTO_TCP, TCP_KEEPINTVL, &keepalive_intvl, sizeof(keepalive_intvl)) < 0) {
        INFRA_LOG_WARN("Failed to set TCP_KEEPINTVL");
    }
#endif

#ifdef TCP_KEEPCNT
    int keepalive_probes = 6;  // 最多发送 6 次 keepalive
    if (setsockopt(client, IPPROTO_TCP, TCP_KEEPCNT, &keepalive_probes, sizeof(keepalive_probes)) < 0) {
        INFRA_LOG_WARN("Failed to set TCP_KEEPCNT");
    }
#endif

    // 初始化数据库连接
    err = db_init(&conn->store);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize database connection");
        infra_free(conn->rx_buf);
        infra_free(conn);
        infra_net_close(client);
        return;
    }

    // 设置连接状态
    conn->sock = client;
    conn->should_close = false;
    conn->is_closing = false;
    conn->is_initialized = true;
    conn->total_commands = 0;
    conn->failed_commands = 0;
    conn->rx_len = 0;
    conn->last_active_time = time(NULL);

    INFRA_LOG_INFO("New client connection from %s", conn->client_addr);

    // 更新处理器参数
    args->user_data = conn;
}

static void handle_request_wrapper(void* args) {
    poly_poll_handler_args_t* handler_args = (poly_poll_handler_args_t*)args;
    if (!handler_args) {
        INFRA_LOG_ERROR("NULL handler args");
        return;
    }

    memkv_conn_t* conn = (memkv_conn_t*)handler_args->user_data;
    if (!conn) {
        // First time handling this connection, initialize it
        handle_connection((poly_poll_handler_args_t*)args);
        conn = (memkv_conn_t*)handler_args->user_data;
        if (!conn) {
            INFRA_LOG_ERROR("Failed to initialize connection");
            return;
        }
    }

    // 检查数据库连接是否有效
    if (!conn->store) {
        INFRA_LOG_ERROR("Invalid database connection");
        conn->should_close = true;
        return;
    }

    handle_request(args);

    // 如果连接需要关闭，清理资源
    if (conn->should_close) {
        INFRA_LOG_INFO("Closing connection from %s", conn->client_addr);
        if (conn->store) {
            poly_db_close(conn->store);
            conn->store = NULL;
        }
        if (conn->rx_buf) {
            infra_free(conn->rx_buf);
            conn->rx_buf = NULL;
        }
        infra_free(conn);
        handler_args->user_data = NULL;
    }
}

//-----------------------------------------------------------------------------
// Service Interface Implementation
//-----------------------------------------------------------------------------

infra_error_t memkv_init(void) {
    INFRA_LOG_INFO("Initializing MemKV service...");
    
    if (g_memkv_service.state != PEER_SERVICE_STATE_INIT &&
        g_memkv_service.state != PEER_SERVICE_STATE_STOPPED) {
        INFRA_LOG_ERROR("Invalid service state: %d", g_memkv_service.state);
        return INFRA_ERROR_INVALID_STATE;
    }
    
    // 分配并初始化状态结构
    memkv_state_t* state = (memkv_state_t*)infra_malloc(sizeof(memkv_state_t));
    if (!state) {
        INFRA_LOG_ERROR("Failed to allocate service state");
        return INFRA_ERROR_NO_MEMORY;
    }
    
    // 初始化状态
    memset(state, 0, sizeof(memkv_state_t));
    state->port = MEMKV_DEFAULT_PORT;
    strncpy(state->host, "127.0.0.1", sizeof(state->host) - 1);
    strncpy(state->engine, "sqlite", sizeof(state->engine) - 1);
    state->running = false;
    state->ctx = NULL;
    
    // 初始化互斥锁
    infra_error_t err = infra_mutex_create(&state->mutex);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize mutex");
        infra_free(state);
        return err;
    }
    
    // 保存状态
    g_memkv_service.config.user_data = state;
    g_memkv_service.state = PEER_SERVICE_STATE_READY;
    
    INFRA_LOG_INFO("MemKV service initialized successfully");
    return INFRA_OK;
}

infra_error_t memkv_cleanup(void) {
    if (g_memkv_service.state == PEER_SERVICE_STATE_RUNNING) {
        return INFRA_ERROR_INVALID_STATE;
    }

    memkv_state_t* state = get_state();
    if (!state) {
        return INFRA_OK;  // 已经清理
    }

    if (state->running) {
        memkv_stop();
    }

    // 清理资源 - 使用 memset 清空数组
    memset(state->engine, 0, sizeof(state->engine));
    memset(state->plugin, 0, sizeof(state->plugin));

    // 销毁互斥锁
    infra_mutex_destroy(&state->mutex);

    // 释放状态结构
    infra_free(state);
    g_memkv_service.config.user_data = NULL;
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

    memkv_state_t* state = get_state();
    if (!state) {
        INFRA_LOG_ERROR("Service state not initialized");
        return INFRA_ERROR_INVALID_STATE;
    }

    if (state->running) {
        INFRA_LOG_ERROR("Service is already running");
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    // Create poll context
    state->ctx = (poly_poll_context_t*)infra_malloc(sizeof(poly_poll_context_t));
    if (!state->ctx) {
        INFRA_LOG_ERROR("Failed to allocate poll context");
        return INFRA_ERROR_NO_MEMORY;
    }
    memset(state->ctx, 0, sizeof(poly_poll_context_t));

    // Initialize poll context
    poly_poll_config_t config = {
        .min_threads = 2,
        .max_threads = MEMKV_MAX_THREADS,
        .queue_size = 1000,
        .max_listeners = 1,
        .read_buffer_size = MEMKV_BUFFER_SIZE
    };

    err = poly_poll_init(state->ctx, &config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize poll context: %d", err);
        infra_free(state->ctx);
        state->ctx = NULL;
        return err;
    }

    // Set request handler
    poly_poll_set_handler(state->ctx, handle_request_wrapper);

    // Add listener
    poly_poll_listener_t listener = {0};
    listener.bind_port = state->port;
    strncpy(listener.bind_addr, state->host[0] ? state->host : "0.0.0.0", sizeof(listener.bind_addr) - 1);
    listener.bind_addr[sizeof(listener.bind_addr) - 1] = '\0';

    err = poly_poll_add_listener(state->ctx, &listener);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to add listener: %d", err);
        poly_poll_cleanup(state->ctx);
        infra_free(state->ctx);
        state->ctx = NULL;
        return err;
    }

    // Start polling
    err = poly_poll_start(state->ctx);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to start polling: %d", err);
        poly_poll_cleanup(state->ctx);
        infra_free(state->ctx);
        state->ctx = NULL;
        return err;
    }

    state->running = true;
    g_memkv_service.state = PEER_SERVICE_STATE_RUNNING;
    
    INFRA_LOG_INFO("MemKV service started successfully on %s:%d", 
                   listener.bind_addr, listener.bind_port);
    return INFRA_OK;
}

infra_error_t memkv_stop(void) {
    if (g_memkv_service.state != PEER_SERVICE_STATE_RUNNING) {
        return INFRA_ERROR_INVALID_STATE;
    }

    memkv_state_t* state = get_state();
    if (!state) {
        return INFRA_ERROR_INVALID_STATE;
    }

    if (!state->running) {
        return INFRA_OK;
    }

    state->running = false;

    if (state->ctx) {
        poly_poll_stop(state->ctx);
        poly_poll_cleanup(state->ctx);
        infra_free(state->ctx);
        state->ctx = NULL;
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
        snprintf(response, size, "Error: Empty command");
        return INFRA_ERROR_INVALID_PARAM;
    }

    memkv_state_t* state = get_state();

    if (strcmp(argv[0], "status") == 0) {
        const char* state_str = "unknown";
        switch (g_memkv_service.state) {
            case PEER_SERVICE_STATE_INIT: state_str = "initialized"; break;
            case PEER_SERVICE_STATE_READY: state_str = "ready"; break;
            case PEER_SERVICE_STATE_RUNNING: state_str = "running"; break;
            case PEER_SERVICE_STATE_STOPPED: state_str = "stopped"; break;
        }
        snprintf(response, size, "MemKV Service Status:\n"
                "State: %s\n"
                "Port: %d\n"
                "Engine: %s\n"
                "Plugin: %s\n",
                state_str,
                state ? state->port : MEMKV_DEFAULT_PORT,
                state && state->engine ? state->engine : "none",
                state && state->plugin ? state->plugin : "none");
        return INFRA_OK;
    }
    else if (strcmp(argv[0], "start") == 0) {
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

infra_error_t memkv_apply_config(const poly_service_config_t* config) {
    INFRA_LOG_INFO("Applying configuration...");
    
    if (!config) {
        INFRA_LOG_ERROR("Invalid configuration");
        return INFRA_ERROR_INVALID_PARAM;
    }

    memkv_state_t* state = get_state();
    if (!state) {
        INFRA_LOG_ERROR("Service state not initialized");
        return INFRA_ERROR_INVALID_STATE;
    }

    if (g_memkv_service.state != PEER_SERVICE_STATE_READY) {
        INFRA_LOG_ERROR("Service in invalid state: %d", g_memkv_service.state);
        return INFRA_ERROR_INVALID_STATE;
    }

    // 从配置中获取服务配置
    strncpy(state->host, config->listen_host, sizeof(state->host) - 1);
    state->host[sizeof(state->host) - 1] = '\0';
    state->port = config->listen_port > 0 ? config->listen_port : MEMKV_DEFAULT_PORT;
    
    // 如果提供了后端配置,使用它作为存储引擎
    if (config->backend && config->backend[0]) {
        strncpy(state->engine, config->backend, sizeof(state->engine) - 1);
        state->engine[sizeof(state->engine) - 1] = '\0';
    }

    INFRA_LOG_INFO("Applied configuration - host: %s, port: %d, engine: %s",
        state->host, state->port, state->engine);

    return INFRA_OK;
}

peer_service_t* peer_memkv_get_service(void) {
    return &g_memkv_service;
}

