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
static void handle_request(memkv_conn_t* conn);
static void handle_connection(poly_poll_handler_args_t* args);
static int handle_get(memkv_conn_t* conn, const char* key);
static void handle_delete(memkv_conn_t* conn, const char* key, bool noreply);
static void handle_flush(memkv_conn_t* conn, bool noreply);
static void handle_incr_decr(memkv_conn_t* conn, const char* key, const char* value_str, bool is_incr);

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define MEMKV_VERSION "1.0.0"
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
    
    infra_error_t err = poly_db_open(&config, db);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to open database connection: %d", err);
        return err;
    }

    // 启用 WAL 模式以提高并发性能
    err = poly_db_exec(*db, "PRAGMA journal_mode=WAL;");
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to enable WAL mode");
        poly_db_close(*db);
        *db = NULL;
        return err;
    }

    // 设置较短的超时和重试
    err = poly_db_exec(*db, "PRAGMA busy_timeout=5000;");
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set busy timeout");
        poly_db_close(*db);
        *db = NULL;
        return err;
    }

    // 设置共享缓存模式
    err = poly_db_exec(*db, "PRAGMA cache_size=2000;");  // 2000 pages = ~8MB cache
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set cache size");
        poly_db_close(*db);
        *db = NULL;
        return err;
    }

    err = poly_db_exec(*db, "PRAGMA synchronous=NORMAL;");  // 提高写入性能
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set synchronous mode");
        poly_db_close(*db);
        *db = NULL;
        return err;
    }

    err = poly_db_exec(*db, "PRAGMA locking_mode=NORMAL;");  // 使用正常的锁定模式
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set locking mode");
        poly_db_close(*db);
        *db = NULL;
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
        return err;
    }
    
    INFRA_LOG_INFO("Database connection established");
    return INFRA_OK;
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
    if (err != INFRA_OK) {
        printf("DEBUG: Key not found or error: [%s], err=%d\n", key, err);
        poly_db_stmt_finalize(stmt);
        return INFRA_ERROR_NOT_FOUND;
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
    if (!conn || !key || !conn->store) {
        INFRA_LOG_ERROR("Invalid parameters");
        return -1;
    }

    // 获取键值对
    size_t value_len = 0;
    uint32_t flags = 0;
    uint32_t exptime = 0;  // 添加过期时间参数
    void* value = NULL;
    infra_error_t err = kv_get(conn->store, key, &value, &value_len, &flags, &exptime);
    
    if (err != INFRA_OK) {
        if (err == INFRA_ERROR_NOT_FOUND) {
            INFRA_LOG_DEBUG("Key not found: %s", key);
            err = send_all(conn->sock, "END\r\n", 5);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send END response: %d", err);
                conn->should_close = true;
            }
            return 0;  // 键不存在，但不是错误
        }
        INFRA_LOG_ERROR("Failed to get value for key %s: %d", key, err);
        return -1;
    }

    if (!value) {
        INFRA_LOG_ERROR("Value is NULL for key %s", key);
        return -1;
    }

    // 发送响应头
    char header[256];
    int header_len = snprintf(header, sizeof(header), "VALUE %s %u %zu\r\n", 
                            key, flags, value_len);
    if (header_len < 0 || header_len >= (int)sizeof(header)) {
        INFRA_LOG_ERROR("Failed to format response header");
        free(value);
        return -1;
    }

    // 发送响应头
    err = send_all(conn->sock, header, header_len);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to send response header: %d", err);
        free(value);
        return -1;
    }

    // 发送值
    err = send_all(conn->sock, value, value_len);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to send value: %d", err);
        free(value);
        return -1;
    }

    // 发送值的结束标记
    err = send_all(conn->sock, "\r\n", 2);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to send value terminator: %d", err);
        free(value);
        return -1;
    }

    // 发送 END 标记
    err = send_all(conn->sock, "END\r\n", 5);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to send END marker: %d", err);
        free(value);
        return -1;
    }

    INFRA_LOG_INFO("Successfully sent key-value pair: [%s]=[%.*s]", 
                   key, (int)value_len, (char*)value);

    free(value);
    return 1;  // 返回1表示成功找到并发送了键值对
}

static void handle_request(memkv_conn_t* conn) {
    if (!conn || !conn->rx_buf || conn->sock <= 0) {
        INFRA_LOG_ERROR("Invalid parameters");
        return;
    }

    // 接收数据
    size_t received = 0;
    infra_error_t err = infra_net_recv(conn->sock, 
        conn->rx_buf + conn->rx_len, 
        MEMKV_CONN_BUFFER_SIZE - conn->rx_len - 1, 
        &received);
    
    if (err != INFRA_OK) {
        if (err == INFRA_ERROR_WOULD_BLOCK) {
            return;  // 非阻塞模式下没有数据可读
        }
        if (err == INFRA_ERROR_TIMEOUT) {
            return;  // 读取超时
        }
        INFRA_LOG_ERROR("Failed to receive data from %s: %d", conn->client_addr, err);
        conn->should_close = true;
        return;
    }
    
    if (received == 0) {
        INFRA_LOG_INFO("Client %s disconnected", conn->client_addr);
        conn->should_close = true;
        return;
    }

    conn->rx_len += received;
    conn->rx_buf[conn->rx_len] = '\0';  // 确保字符串以 null 结尾
    conn->last_active_time = time(NULL);

    INFRA_LOG_INFO("Received data from %s: [%s]", conn->client_addr, conn->rx_buf);

    // 处理接收到的命令
    char* line = conn->rx_buf;
    char* next_line;
    char* data_line = NULL;
    size_t data_len = 0;
    bool in_data = false;
    
    while (line && *line) {
        // 查找下一行
        next_line = strstr(line, "\r\n");
        if (!next_line) {
            // 命令不完整，等待更多数据
            if (line != conn->rx_buf) {
                // 移动未完成的命令到缓冲区开始
                size_t remaining = conn->rx_len - (line - conn->rx_buf);
                memmove(conn->rx_buf, line, remaining);
                conn->rx_len = remaining;
            }
            return;
        }
        *next_line = '\0';  // 将 \r\n 替换为字符串结束符
        next_line += 2;     // 跳过 \r\n

        // 跳过空行
        if (*line == '\0') {
            line = next_line;
            continue;
        }

        // 如果正在处理 SET 命令的数据部分
        if (in_data) {
            data_line = line;
            data_len = next_line - line - 2;  // 减去 \r\n
            in_data = false;
            
            // 验证数据长度
            if (data_len != conn->set_bytes) {
                INFRA_LOG_ERROR("Data length mismatch: expected %zu, got %zu", 
                               conn->set_bytes, data_len);
                if (!conn->set_noreply) {
                    err = send_all(conn->sock, "CLIENT_ERROR bad data chunk\r\n", 26);
                    if (err != INFRA_OK) {
                        INFRA_LOG_ERROR("Failed to send error response: %d", err);
                        conn->should_close = true;
                    }
                }
                line = next_line;
                continue;
            }

            // 保存数据
            err = kv_set(conn->store, conn->set_key, data_line, data_len, 
                        conn->set_flags, conn->set_exptime);
            
            if (err == INFRA_OK) {
                if (!conn->set_noreply) {
                    err = send_all(conn->sock, "STORED\r\n", 8);
                    if (err != INFRA_OK) {
                        INFRA_LOG_ERROR("Failed to send response: %d", err);
                        conn->should_close = true;
                    }
                }
            } else {
                INFRA_LOG_ERROR("Failed to set key-value pair: %d", err);
                if (!conn->set_noreply) {
                    err = send_all(conn->sock, "SERVER_ERROR\r\n", 14);
                    if (err != INFRA_OK) {
                        INFRA_LOG_ERROR("Failed to send error response: %d", err);
                        conn->should_close = true;
                    }
                }
            }

            line = next_line;
            continue;
        }

        // 解析命令
        char cmd[32] = {0};
        char key[256] = {0};
        
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

        INFRA_LOG_DEBUG("Processing command from %s: %s, key: %s", conn->client_addr, cmd, key);
        conn->total_commands++;

        // 处理命令
        if (strcmp(cmd, "get") == 0) {
            // 检查是否有更多的参数
            char* next_key = key;
            bool found = false;
            while (next_key && *next_key) {
                // 处理当前 key
                int result = handle_get(conn, next_key);
                if (result > 0) found = true;
                
                // 查找下一个 key
                char* space = strchr(next_key, ' ');
                if (space) {
                    *space = '\0';
                    next_key = space + 1;
                    while (*next_key == ' ') next_key++;  // 跳过多余的空格
                } else {
                    next_key = NULL;
                }
            }
            
            // 如果没有找到任何键，发送 END 标记
            if (!found) {
                err = send_all(conn->sock, "END\r\n", 5);
                if (err != INFRA_OK) {
                    INFRA_LOG_ERROR("Failed to send END marker: %d", err);
                    conn->should_close = true;
                }
            }
            line = next_line;
        }
        else if (strcmp(cmd, "set") == 0) {
            char flags_str[32] = {0};
            char exptime_str[32] = {0};
            char bytes_str[32] = {0};
            char noreply_str[32] = {0};
            int matched = sscanf(line, "%*s %*s %31s %31s %31s %31s", 
                               flags_str, exptime_str, bytes_str, noreply_str);
            
            if (matched >= 3) {
                conn->set_flags = (uint32_t)strtoul(flags_str, NULL, 10);
                conn->set_exptime = (uint32_t)strtoul(exptime_str, NULL, 10);
                conn->set_bytes = (size_t)strtoul(bytes_str, NULL, 10);
                
                if (conn->set_bytes > MEMKV_MAX_DATA_SIZE) {
                    INFRA_LOG_ERROR("Value too large from %s: %zu", conn->client_addr, conn->set_bytes);
                    if (conn->sock > 0) {
                        err = send_all(conn->sock, "SERVER_ERROR value too large\r\n", 28);
                        if (err != INFRA_OK) {
                            INFRA_LOG_ERROR("Failed to send error response: %d", err);
                            conn->should_close = true;
                        }
                    }
                    line = next_line;
                    continue;
                }

                conn->set_noreply = (matched == 4 && strcmp(noreply_str, "noreply") == 0);
                strncpy(conn->set_key, key, sizeof(conn->set_key) - 1);
                in_data = true;
                line = next_line;
            } else {
                INFRA_LOG_ERROR("Invalid SET command format from %s: [%s]", conn->client_addr, line);
                if (conn->sock > 0) {
                    err = send_all(conn->sock, "CLIENT_ERROR bad command line format\r\n", 37);
                    if (err != INFRA_OK) {
                        INFRA_LOG_ERROR("Failed to send error response: %d", err);
                        conn->should_close = true;
                    }
                }
                line = next_line;
            }
        }
        else if (strcmp(cmd, "delete") == 0) {
            char noreply_str[32] = {0};
            bool noreply = (sscanf(line, "%*s %*s %31s", noreply_str) == 1 && 
                           strcmp(noreply_str, "noreply") == 0);
            handle_delete(conn, key, noreply);
            line = next_line;
        }
        else if (strcmp(cmd, "flush_all") == 0) {
            char noreply_str[32] = {0};
            bool noreply = (sscanf(line, "%*s %*s %31s", noreply_str) == 1 && 
                           strcmp(noreply_str, "noreply") == 0);
            handle_flush(conn, noreply);
            line = next_line;
        }
        else if (strcmp(cmd, "incr") == 0 || strcmp(cmd, "decr") == 0) {
            char value_str[32] = {0};
            if (sscanf(line, "%*s %*s %31s", value_str) == 1) {
                handle_incr_decr(conn, key, value_str, cmd[0] == 'i');
            } else {
                INFRA_LOG_ERROR("Invalid INCR/DECR command format from %s: [%s]", conn->client_addr, line);
                conn->failed_commands++;
                if (conn->sock > 0) {
                    err = send_all(conn->sock, "CLIENT_ERROR bad command line format\r\n", 37);
                    if (err != INFRA_OK) {
                        INFRA_LOG_ERROR("Failed to send error response: %d", err);
                        conn->should_close = true;
                    }
                }
            }
            line = next_line;
        }
        else {
            INFRA_LOG_ERROR("Unknown command from %s: [%s]", conn->client_addr, cmd);
            if (conn->sock > 0) {
                err = send_all(conn->sock, "ERROR\r\n", 7);
                if (err != INFRA_OK) {
                    INFRA_LOG_ERROR("Failed to send error response: %d", err);
                    conn->should_close = true;
                }
            }
            line = next_line;
        }
    }

    // 更新缓冲区
    if (line != conn->rx_buf) {
        size_t processed = line - conn->rx_buf;
        if (processed < conn->rx_len) {
            size_t remaining = conn->rx_len - processed;
            memmove(conn->rx_buf, line, remaining);
            conn->rx_len = remaining;
        } else {
            conn->rx_len = 0;
        }
    }
    
    // 如果缓冲区已经处理完毕，重置它
    if (conn->rx_len == 0) {
        conn->rx_buf[0] = '\0';
    }
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
        memkv_conn_destroy(conn);
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
        memkv_conn_destroy(conn);
        return;
    }

    // 设置 TCP keepalive
    flag = 1;
    if (setsockopt(client, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) < 0) {
        INFRA_LOG_ERROR("Failed to set SO_KEEPALIVE");
        memkv_conn_destroy(conn);
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
        memkv_conn_destroy(conn);
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

static void handle_connection_wrapper(void* args) {
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
    }

    if (!conn->should_close) {
        // 处理接收到的数据
        handle_request(conn);
    }

    // 如果连接需要关闭，清理资源
    if (conn->should_close) {
        INFRA_LOG_INFO("Closing connection from %s", conn->client_addr);
        memkv_conn_destroy(conn);
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
    strncpy(state->db_path, ":memory:", sizeof(state->db_path) - 1);
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
    memset(state->db_path, 0, sizeof(state->db_path));

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
        .read_buffer_size = MEMKV_CONN_BUFFER_SIZE
    };

    err = poly_poll_init(state->ctx, &config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize poll context: %d", err);
        infra_free(state->ctx);
        state->ctx = NULL;
        return err;
    }

    // Set request handler
    poly_poll_set_handler(state->ctx, handle_connection_wrapper);

    // Add listener
    poly_poll_listener_t listener = {0};
    if (!state->host[0]) {
        strncpy(state->host, "127.0.0.1", sizeof(state->host) - 1);
    }
    strncpy(listener.bind_addr, state->host, sizeof(listener.bind_addr) - 1);
    listener.bind_port = state->port;
    listener.user_data = NULL;

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
                   state->host, state->port);
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
                "DB Path: %s\n",
                state_str,
                state ? state->port : MEMKV_DEFAULT_PORT,
                state && state->db_path ? state->db_path : "none");
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
    
    // 如果提供了后端配置,使用它作为数据库路径
    if (config->backend && config->backend[0]) {
        strncpy(state->db_path, config->backend, sizeof(state->db_path) - 1);
        state->db_path[sizeof(state->db_path) - 1] = '\0';
    }

    INFRA_LOG_INFO("Applied configuration - host: %s, port: %d, db_path: %s",
        state->host, state->port, state->db_path);

    return INFRA_OK;
}

peer_service_t* peer_memkv_get_service(void) {
    return &g_memkv_service;
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

    infra_error_t err = kv_delete(conn->store, key);
    if (err == INFRA_OK) {
        if (!noreply) {
            err = send_all(conn->sock, "DELETED\r\n", 9);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send DELETED response: %d", err);
                conn->should_close = true;
            }
        }
    } else if (err == INFRA_ERROR_NOT_FOUND) {
        if (!noreply) {
            err = send_all(conn->sock, "NOT_FOUND\r\n", 11);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send NOT_FOUND response: %d", err);
                conn->should_close = true;
            }
        }
    } else {
        if (!noreply) {
            err = send_all(conn->sock, "SERVER_ERROR\r\n", 14);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send SERVER_ERROR response: %d", err);
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

    infra_error_t err = kv_flush(conn->store);
    if (!noreply) {
        if (err == INFRA_OK) {
            err = send_all(conn->sock, "OK\r\n", 4);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send OK response: %d", err);
                conn->should_close = true;
            }
        } else {
            err = send_all(conn->sock, "SERVER_ERROR\r\n", 14);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send SERVER_ERROR response: %d", err);
                conn->should_close = true;
            }
        }
    }
}

static void handle_incr_decr(memkv_conn_t* conn, const char* key, const char* value_str, bool is_incr) {
    if (!conn || !conn->store || !key || !value_str || conn->sock <= 0) {
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
    uint32_t exptime = 0;
    
    infra_error_t err = kv_get(conn->store, key, &old_value, &old_value_len, &flags, &exptime);
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
    if (new_value_len < 0 || (size_t)new_value_len >= sizeof(new_value)) {
        INFRA_LOG_ERROR("Failed to format new value");
        err = send_all(conn->sock, "SERVER_ERROR value too large\r\n", 28);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to send error response: %d", err);
            conn->should_close = true;
        }
        return;
    }
    
    err = kv_set(conn->store, key, new_value, new_value_len, flags, 0);
    if (err == INFRA_OK) {
        char response[32];
        int response_len = snprintf(response, sizeof(response), "%lu\r\n", current);
        if (response_len < 0 || (size_t)response_len >= sizeof(response)) {
            INFRA_LOG_ERROR("Failed to format response");
            err = send_all(conn->sock, "SERVER_ERROR response too large\r\n", 30);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to send error response: %d", err);
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
        err = send_all(conn->sock, "ERROR\r\n", 7);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to send ERROR response: %d", err);
            conn->should_close = true;
        }
    }
}

static void memkv_conn_destroy(memkv_conn_t* conn) {
    if (!conn) {
        return;
    }

    // 关闭数据库连接
    if (conn->store) {
        poly_db_close(conn->store);
        conn->store = NULL;
    }

    // 关闭套接字
    if (conn->sock > 0) {
        infra_net_close(conn->sock);
        conn->sock = 0;
    }

    // 释放接收缓冲区
    if (conn->rx_buf) {
        infra_free(conn->rx_buf);
        conn->rx_buf = NULL;
    }

    // 清空客户端地址
    memset(conn->client_addr, 0, sizeof(conn->client_addr));

    // 重置其他字段
    conn->rx_len = 0;
    conn->should_close = false;
    conn->is_closing = false;
    conn->is_initialized = false;
    conn->total_commands = 0;
    conn->failed_commands = 0;
    conn->last_active_time = 0;

    // 最后释放连接结构体
    infra_free(conn);
}

