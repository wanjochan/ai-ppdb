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

// Forward declarations
static void handle_connection(poly_poll_handler_args_t* args);
static void handle_connection_wrapper(void* args);
static void handle_request_wrapper(void* args);
static infra_error_t kv_delete(poly_db_t* db, const char* key);

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
// Types
//-----------------------------------------------------------------------------

typedef struct {
    infra_socket_t sock;              // Client socket
    poly_db_t* store;                 // Database connection
    char* rx_buf;                     // Receive buffer
    size_t rx_len;                    // Current buffer length
    size_t rx_pos;                    // Current read position
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
    char db_path[256];               // Database path
    char host[64];                   // Host to bind to
    int port;                        // Port to bind to
    char engine[32];                 // Storage engine
    char plugin[256];                // Plugin path
    volatile bool running;           // Service running flag
    infra_mutex_t mutex;             // Service mutex
    poly_poll_context_t* ctx;        // Poll context
} memkv_state_t;

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
                           size_t* value_len, uint32_t* flags) {
    const char* sql = 
        "SELECT value, flags, expiry FROM kv_store WHERE key = ?";
    
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
        return INFRA_ERROR_NOT_FOUND;
    }
        
    // 检查过期时间
    char* expiry_str = NULL;
    err = poly_db_column_text(stmt, 2, &expiry_str);
    if (err == INFRA_OK && expiry_str) {
        time_t expiry = strtol(expiry_str, NULL, 10);
        free(expiry_str);
        if (expiry > 0 && expiry <= time(NULL)) {
            // 已过期，删除并返回未找到
            kv_delete(db, key);
            poly_db_stmt_finalize(stmt);
            return INFRA_ERROR_NOT_FOUND;
        }
    }
        
    // 获取值和标志
    err = poly_db_column_blob(stmt, 0, value, value_len);
    if (err != INFRA_OK) {
        poly_db_stmt_finalize(stmt);
        return err;
    }
        
    char* flags_str = NULL;
    err = poly_db_column_text(stmt, 1, &flags_str);
    if (err == INFRA_OK && flags_str) {
        *flags = strtoul(flags_str, NULL, 10);
        free(flags_str);
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

static infra_error_t send_all(infra_socket_t sock, const void* data, size_t len) {
    size_t sent = 0;
    int retry_count = 0;
    const int max_retries = 3;
    
    while (sent < len) {
        size_t bytes_sent = 0;
        infra_error_t err = infra_net_send(sock, (const char*)data + sent, len - sent, &bytes_sent);
        
        if (err == INFRA_ERROR_WOULD_BLOCK) {
            if (retry_count < max_retries) {
                // 短暂等待后重试
                usleep(10000); // 10ms
                retry_count++;
                continue;
            }
            printf("DEBUG: Send would block after %d retries\n", max_retries);
            return err;
        }
        
        if (err != INFRA_OK || bytes_sent == 0) {
            printf("DEBUG: Failed to send data: err=%d\n", err);
            return err;
        }
        
        sent += bytes_sent;
        retry_count = 0; // 重置重试计数
    }
    return INFRA_OK;
}

static void handle_get(memkv_conn_t* conn, const char* key) {
    if (!conn || !conn->store || !key) {
        INFRA_LOG_ERROR("Invalid parameters in handle_get");
        return;
    }

    printf("DEBUG: Handling GET command for key='%s'\n", key);
    
    void* value = NULL;
    size_t value_len = 0;
    uint32_t flags = 0;

    infra_error_t err = kv_get(conn->store, key, &value, &value_len, &flags);
    printf("DEBUG: GET result: err=%d, value=%p, value_len=%zu\n", err, value, value_len);
    
    if (err == INFRA_OK && value && value_len > 0) {
        // 发送头部
        char header[128];
        int header_len = snprintf(header, sizeof(header), 
                                "VALUE %s %u %zu\r\n", key, flags, value_len);
        
        err = send_all(conn->sock, header, header_len);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to send header: %d", err);
            conn->should_close = true;
            infra_free(value);
            return;
        }
        
        // 发送值
        err = send_all(conn->sock, value, value_len);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to send value: %d", err);
            conn->should_close = true;
            infra_free(value);
            return;
        }
        
        // 发送行结束符
        err = send_all(conn->sock, "\r\n", 2);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to send line ending: %d", err);
            conn->should_close = true;
            infra_free(value);
            return;
        }
        
        // 清理资源
        infra_free(value);
    }
    
    printf("DEBUG: GET command completed for key='%s', should_close=%d\n", key, conn->should_close);
}

static void handle_set(memkv_conn_t* conn, const char* key, const char* flags_str,
                      const char* exptime_str, const char* bytes_str, bool noreply,
                      const char* data_ptr) {
    infra_error_t err = INFRA_OK;
    
    printf("DEBUG: Handling SET command: key='%s', flags='%s', exptime='%s', bytes='%s'\n",
           key, flags_str, exptime_str, bytes_str);
           
    uint32_t flags = strtoul(flags_str, NULL, 10);
    time_t exptime = strtol(exptime_str, NULL, 10);
    size_t bytes = strtoul(bytes_str, NULL, 10);

    printf("DEBUG: Parsed SET params: flags=%u, exptime=%ld, bytes=%zu\n",
           flags, exptime, bytes);

    // 检查数据大小限制
    if (bytes > MEMKV_MAX_DATA_SIZE) {
        printf("DEBUG: Data too large (max %d bytes)\n", MEMKV_MAX_DATA_SIZE);
        if (!noreply) {
            err = send_all(conn->sock, "SERVER_ERROR object too large\r\n", 30);
            if (err != INFRA_OK) {
                printf("DEBUG: Failed to send error response: err=%d\n", err);
                conn->should_close = true;
            }
        }
        return;
    }

    // 使用传入的数据指针
    if (!data_ptr) {
        printf("DEBUG: SET failed - no data provided\n");
        if (!noreply) {
            err = send_all(conn->sock, "CLIENT_ERROR bad data chunk\r\n", 28);
            if (err != INFRA_OK) {
                printf("DEBUG: Failed to send error response: err=%d\n", err);
                conn->should_close = true;
            }
        }
        return;
    }

    if (exptime > 0 && exptime < 2592000) {
        exptime += time(NULL);
    }

    err = kv_set(conn->store, key, data_ptr, bytes, flags, exptime);
    printf("DEBUG: SET storage result: err=%d\n", err);

    if (!noreply) {
        if (err == INFRA_OK) {
            err = send_all(conn->sock, "STORED\r\n", 8);
            if (err != INFRA_OK) {
                printf("DEBUG: Failed to send STORED response: err=%d\n", err);
                conn->should_close = true;
            }
        } else {
            err = send_all(conn->sock, "NOT_STORED\r\n", 12);
            if (err != INFRA_OK) {
                printf("DEBUG: Failed to send NOT_STORED response: err=%d\n", err);
                conn->should_close = true;
            }
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
    
    infra_error_t err = kv_get(conn->store, key, &old_value, &old_value_len, &flags);
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

static void handle_request(void* args) {
    poly_poll_handler_args_t* handler_args = (poly_poll_handler_args_t*)args;
    if (!handler_args || !handler_args->user_data) {
        INFRA_LOG_ERROR("Invalid handler args");
        return;
    }
    
    memkv_conn_t* conn = (memkv_conn_t*)handler_args->user_data;
    if (!conn->store) {
        INFRA_LOG_ERROR("Invalid connection store");
        return;
    }

    conn->sock = handler_args->client;  // Update socket handle
    
    char cmd[32] = {0};
    char key[256] = {0};
    char flags_str[32] = {0};
    char exptime_str[32] = {0};
    char bytes_str[32] = {0};
    char temp_buffer[MEMKV_BUFFER_SIZE] = {0};
    size_t received = 0;
    
    // 接收新数据
    infra_error_t err = infra_net_recv(conn->sock, temp_buffer, sizeof(temp_buffer)-1, &received);
    if (err != INFRA_OK || received == 0) {
        if (err == INFRA_ERROR_CLOSED || received == 0) {
            INFRA_LOG_INFO("Client disconnected");
        } else {
            INFRA_LOG_ERROR("Failed to receive data: %d", err);
        }
        conn->should_close = true;
        return;
    }

    // 检查缓冲区是否有足够空间
    if (conn->rx_len + received > MEMKV_BUFFER_SIZE) {
        // 如果缓冲区已满但还没有找到完整命令，说明命令太长
        if (conn->rx_pos == 0) {
            INFRA_LOG_ERROR("Command too long");
            conn->should_close = true;
            return;
        }
        // 移动未处理的数据到缓冲区开始
        memmove(conn->rx_buf, conn->rx_buf + conn->rx_pos, conn->rx_len - conn->rx_pos);
        conn->rx_len -= conn->rx_pos;
        conn->rx_pos = 0;
    }

    // 追加新数据到缓冲区
    memcpy(conn->rx_buf + conn->rx_len, temp_buffer, received);
    conn->rx_len += received;
    conn->rx_buf[conn->rx_len] = '\0';

    printf("DEBUG: Received data: [%.*s]\n", (int)received, temp_buffer);
    printf("DEBUG: Current buffer: [%.*s]\n", (int)conn->rx_len, conn->rx_buf);
    
    char* line = conn->rx_buf + conn->rx_pos;
    char* next_line;
    bool noreply = false;
    bool should_continue = true;

    while (line < conn->rx_buf + conn->rx_len && should_continue) {
        // 找到当前行的结束位置
        next_line = strstr(line, "\r\n");
        if (!next_line) {
            // 没有找到完整的行，等待更多数据
            printf("DEBUG: Incomplete command, waiting for more data\n");
            // 将未完成的数据移到缓冲区开始
            if (line > conn->rx_buf) {
                size_t remaining = conn->rx_len - (line - conn->rx_buf);
                memmove(conn->rx_buf, line, remaining);
                conn->rx_len = remaining;
                conn->rx_pos = 0;
            }
            break;
        }

        printf("DEBUG: Found command line: [%.*s]\n", (int)(next_line - line), line);
        *next_line = '\0';

        // 检查是否有 noreply 参数
        char* noreply_ptr = strstr(line, " noreply");
        if (noreply_ptr) {
            printf("DEBUG: Found noreply parameter\n");
            *noreply_ptr = '\0';
            noreply = true;
        } else {
            noreply = false;
        }

        // 解析命令
        char* cmd_start = line;
        while (*cmd_start == ' ') cmd_start++;  // 跳过前导空格
        
        char* cmd_end = cmd_start;
        while (*cmd_end && *cmd_end != ' ' && *cmd_end != '\r' && *cmd_end != '\n') cmd_end++;  // 找到命令结束位置
        
        if (cmd_end > cmd_start) {
            size_t cmd_len = cmd_end - cmd_start;
            if (cmd_len >= sizeof(cmd)) cmd_len = sizeof(cmd) - 1;
            memcpy(cmd, cmd_start, cmd_len);
            cmd[cmd_len] = '\0';
            
            // 将命令转换为大写
            for (char* p = cmd; *p; p++) {
                *p = toupper(*p);
            }
            
            printf("DEBUG: Parsed command: [%s]\n", cmd);
            
            // 更新下一行的位置
            next_line += 2;  // 跳过 \r\n
            
            if (strcmp(cmd, "GET") == 0) {
                // 处理多键 get 命令
                char* key_start = cmd_end;
                
                // 跳过所有空格
                while (key_start && *key_start == ' ') key_start++;
                
                while (key_start && *key_start && *key_start != '\r' && *key_start != '\n') {
                    // 找到键的结束位置
                    char* key_end = key_start;
                    while (*key_end && *key_end != ' ' && *key_end != '\r' && *key_end != '\n') key_end++;
                    
                    // 提取键
                    size_t key_len = key_end - key_start;
                    if (key_len > 0 && key_len < sizeof(key)) {
                        memcpy(key, key_start, key_len);
                        key[key_len] = '\0';
                        printf("DEBUG: Processing GET for key: [%s]\n", key);
                        handle_get(conn, key);
                        if (conn->should_close) {
                            should_continue = false;
                            break;
                        }
                    }
                    
                    // 移动到下一个键
                    key_start = key_end;
                    while (key_start && *key_start == ' ') key_start++;
                }
                
                // 发送 END 标记
                err = send_all(conn->sock, "END\r\n", 5);
                if (err != INFRA_OK) {
                    INFRA_LOG_ERROR("Failed to send END marker: %d", err);
                    conn->should_close = true;
                    should_continue = false;
                }
            }
            else if (strcmp(cmd, "SET") == 0) {
                // 解析 SET 命令的参数
                char* params_start = cmd_end;
                while (*params_start == ' ') params_start++;  // 跳过空格
                
                if (sscanf(params_start, "%255s %31s %31s %31s", 
                    key, flags_str, exptime_str, bytes_str) == 4) {
                    size_t bytes = strtoul(bytes_str, NULL, 10);
                    
                    // 移动到数据部分
                    line = next_line;
                    
                    // 检查是否有足够的数据
                    if (line + bytes + 2 <= conn->rx_buf + conn->rx_len) {
                        // 确保数据后面跟着 \r\n
                        if (line[bytes] == '\r' && line[bytes + 1] == '\n') {
                            // 暂存数据行的结尾字符
                            char saved_chars[2] = {line[bytes], line[bytes + 1]};
                            line[bytes] = '\0';
                            
                            handle_set(conn, key, flags_str, exptime_str, bytes_str, noreply, line);
                            
                            // 恢复数据行的结尾字符
                            line[bytes] = saved_chars[0];
                            line[bytes + 1] = saved_chars[1];
                            
                            // 移动到下一个命令
                            line = line + bytes + 2;
                            printf("DEBUG: SET command completed\n");
                        } else {
                            printf("DEBUG: Invalid SET data format\n");
                            if (!noreply) {
                                err = send_all(conn->sock, "CLIENT_ERROR bad data chunk\r\n", 28);
                                if (err != INFRA_OK) {
                                    should_continue = false;
                                }
                            }
                            line = next_line;
                        }
                    } else {
                        printf("DEBUG: Incomplete SET data\n");
                        // 将未完成的数据移到缓冲区开始
                        if (line > conn->rx_buf) {
                            size_t remaining = conn->rx_len - (line - conn->rx_buf);
                            memmove(conn->rx_buf, line, remaining);
                            conn->rx_len = remaining;
                            conn->rx_pos = 0;
                        }
                        break;
                    }
                } else {
                    if (!noreply) {
                        err = send_all(conn->sock, "ERROR\r\n", 7);
                        if (err != INFRA_OK) {
                            should_continue = false;
                        }
                    }
                    line = next_line;
                }
            }
            else if (strcmp(cmd, "DELETE") == 0) {
                // 解析 DELETE 命令的参数
                char* params_start = cmd_end;
                while (*params_start == ' ') params_start++;  // 跳过空格
                
                if (sscanf(params_start, "%255s", key) == 1) {
                    handle_delete(conn, key, noreply);
                    if (conn->should_close) {
                        should_continue = false;
                    }
                } else {
                    if (!noreply) {
                        err = send_all(conn->sock, "ERROR\r\n", 7);
                        if (err != INFRA_OK) {
                            should_continue = false;
                        }
                    }
                }
            }
            else if (strcmp(cmd, "FLUSH_ALL") == 0) {
                handle_flush(conn, noreply);
                if (conn->should_close) {
                    should_continue = false;
                }
                line = next_line;
            }
            else if (strcmp(cmd, "INCR") == 0 || strcmp(cmd, "DECR") == 0) {
                // 解析 INCR/DECR 命令的参数
                char* params_start = cmd_end;
                while (*params_start == ' ') params_start++;  // 跳过空格
                
                char value_str[32] = {0};
                if (sscanf(params_start, "%255s %31s", key, value_str) == 2) {
                    handle_incr_decr(conn, key, value_str, cmd[0] == 'I');
                    if (conn->should_close) {
                        should_continue = false;
                    }
                } else {
                    if (!noreply) {
                        err = send_all(conn->sock, "ERROR\r\n", 7);
                        if (err != INFRA_OK) {
                            should_continue = false;
                        }
                    }
                }
            }
            else {
                printf("DEBUG: Unknown command: %s\n", cmd);
                if (!noreply) {
                    err = send_all(conn->sock, "ERROR\r\n", 7);
                    if (err != INFRA_OK) {
                        should_continue = false;
                    }
                }
            }
        }

        // 更新处理位置
        if (should_continue) {
            conn->rx_pos = line - conn->rx_buf;
            printf("DEBUG: Updated buffer position to %zu\n", conn->rx_pos);
        }
    }

    // 如果所有数据都已处理，重置缓冲区
    if (conn->rx_pos >= conn->rx_len) {
        conn->rx_pos = 0;
        conn->rx_len = 0;
        printf("DEBUG: Reset buffer\n");
    } else if (conn->rx_pos > 0) {
        // 移动未处理的数据到缓冲区开始
        size_t remaining = conn->rx_len - conn->rx_pos;
        memmove(conn->rx_buf, conn->rx_buf + conn->rx_pos, remaining);
        conn->rx_len = remaining;
        conn->rx_pos = 0;
        printf("DEBUG: Moved remaining %zu bytes to buffer start\n", remaining);
    }
    
    // Only close if explicitly requested
    if (conn->should_close) {
        INFRA_LOG_INFO("Closing connection");
        poly_db_close(conn->store);
        if (conn->rx_buf) infra_free(conn->rx_buf);
        infra_net_close(conn->sock);
        infra_free(conn);
    }
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

    handle_request(args);
}

static void handle_connection(poly_poll_handler_args_t* args) {
    if (!args) {
        INFRA_LOG_ERROR("NULL handler args");
        return;
    }

    infra_socket_t client = args->client;
    
    // Get client address for logging
    infra_net_addr_t addr;
    infra_error_t err = infra_net_get_peer_addr(client, &addr);
    if (err == INFRA_OK) {
        INFRA_LOG_INFO("New client connection from %s:%d", addr.ip, addr.port);
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
    
    // Initialize database connection
    err = db_init(&conn->store);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize database connection");
        infra_free(conn);
        infra_net_close(client);
        return;
    }

    // Allocate receive buffer
    conn->rx_buf = (char*)infra_malloc(MEMKV_BUFFER_SIZE);
    if (!conn->rx_buf) {
        INFRA_LOG_ERROR("Failed to allocate receive buffer");
        poly_db_close(conn->store);
        infra_free(conn);
        infra_net_close(client);
        return;
    }

    // Store connection context
    args->user_data = conn;

    INFRA_LOG_INFO("Client connection initialized successfully");
}

static void handle_connection_wrapper(void* args) {
    handle_connection((poly_poll_handler_args_t*)args);
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
