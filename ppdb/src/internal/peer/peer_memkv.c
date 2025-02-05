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
// Forward Declarations
//-----------------------------------------------------------------------------

infra_error_t memkv_init(void);
infra_error_t memkv_cleanup(void);
infra_error_t memkv_start(void);
infra_error_t memkv_stop(void);
infra_error_t memkv_cmd_handler(const char* cmd, char* response, size_t size);

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define MEMKV_VERSION "1.0.0"
#define MEMKV_BUFFER_SIZE (64 * 1024 * 1024)  // 增加到64MB
#define MEMKV_MAX_DATA_SIZE (32 * 1024 * 1024)  // 最大数据大小32MB
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
    .cmd_handler = memkv_cmd_handler
};

// Global state
static struct {
    bool running;
    int port;
    char* engine;
    char* plugin;
    poly_poll_context_t* poll_ctx;
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
        printf("DEBUG: Failed to open database: err=%d\n", err);
        return err;
    }

    // 创建 KV 表
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
        printf("DEBUG: Failed to create tables: err=%d\n", err);
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

    err = poly_db_column_blob(stmt, 0, value, value_len);
    if (err == INFRA_OK && flags) {
        char* flags_str = NULL;
        if (poly_db_column_text(stmt, 1, &flags_str) == INFRA_OK && flags_str) {
            *flags = (uint32_t)strtoul(flags_str, NULL, 10);
            free(flags_str);
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
    printf("DEBUG: Handling GET command for key='%s'\n", key);
    
    void* value = NULL;
    size_t value_len = 0;
    uint32_t flags = 0;

    infra_error_t err = kv_get(conn->store, key, &value, &value_len, &flags);
    printf("DEBUG: GET result: err=%d, value=%p, value_len=%zu\n", err, value, value_len);
    
    if (err == INFRA_OK && value) {
        char header[128];
        int header_len = snprintf(header, sizeof(header), 
                                "VALUE %s %u %zu\r\n", key, flags, value_len);
        
        printf("DEBUG: Sending GET response header: [%.*s]\n", header_len-2, header);
        
        // 分块发送大数据
        const size_t chunk_size = 8192; // 8KB chunks
        
        // 发送头部
        err = send_all(conn->sock, header, header_len);
        if (err != INFRA_OK) {
            printf("DEBUG: Failed to send header: err=%d\n", err);
            free(value);
            return;
        }
        
        // 分块发送数据
        size_t sent = 0;
        while (sent < value_len && err == INFRA_OK) {
            size_t remaining = value_len - sent;
            size_t to_send = remaining < chunk_size ? remaining : chunk_size;
            
            err = send_all(conn->sock, (char*)value + sent, to_send);
            if (err != INFRA_OK) {
                printf("DEBUG: Failed to send data chunk at offset %zu: err=%d\n", sent, err);
                break;
            }
            sent += to_send;
        }
        
        // 如果数据发送成功，发送结尾
        if (err == INFRA_OK) {
            err = send_all(conn->sock, "\r\n", 2);
            if (err == INFRA_OK) {
                err = send_all(conn->sock, "END\r\n", 5);
            }
        }
        
        if (err != INFRA_OK) {
            printf("DEBUG: Failed to complete GET response: err=%d\n", err);
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                printf("DEBUG: Send buffer full, connection might be slow\n");
            }
            conn->should_close = true;
        }
        
        free(value);
    } else {
        printf("DEBUG: Key not found or error: %d\n", err);
        send_all(conn->sock, "END\r\n", 5);
    }
}

static void handle_set(memkv_conn_t* conn, const char* key, const char* flags_str,
                      const char* exptime_str, const char* bytes_str, bool noreply) {
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
            infra_net_send(conn->sock, "SERVER_ERROR object too large\r\n", 30, NULL);
        }
        return;
    }

    char* data = malloc(bytes);
    if (!data) {
        printf("DEBUG: SET failed - out of memory\n");
        if (!noreply) {
            infra_net_send(conn->sock, "SERVER_ERROR out of memory\r\n", 26, NULL);
        }
        return;
    }

    size_t total_received = 0;
    int retry_count = 0;
    const int max_retries = 5;  // 增加重试次数
    const size_t chunk_size = 65536;  // 减小到64KB以提高接收成功率
    
    // 设置更长的接收超时
    struct timeval orig_tv, new_tv;
    socklen_t tv_len = sizeof(struct timeval);
    int sock_fd = (int)conn->sock;  // infra_socket_t is intptr_t
    if (sock_fd < 0) {
        printf("DEBUG: Invalid socket fd\n");
        free(data);
        return;
    }
    getsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &orig_tv, &tv_len);
    
    new_tv.tv_sec = 30;  // 减少单次超时时间，但增加重试次数
    new_tv.tv_usec = 0;
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &new_tv, sizeof(new_tv));

    // 临时增加接收缓冲区大小
    int orig_rcvbuf = 0;
    socklen_t optlen = sizeof(orig_rcvbuf);
    getsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &orig_rcvbuf, &optlen);
    
    int large_rcvbuf = MEMKV_BUFFER_SIZE;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &large_rcvbuf, sizeof(large_rcvbuf)) < 0) {
        printf("DEBUG: Failed to set temporary large SO_RCVBUF\n");
    }

    // 设置非阻塞模式
    int flags_orig = fcntl(sock_fd, F_GETFL, 0);
    fcntl(sock_fd, F_SETFL, flags_orig | O_NONBLOCK);

    while (total_received < bytes) {
        size_t to_receive = bytes - total_received;
        if (to_receive > chunk_size) to_receive = chunk_size;
        
        size_t received = 0;
        infra_error_t err = infra_net_recv(conn->sock, 
                                         data + total_received,
                                         to_receive,
                                         &received);
        
        printf("DEBUG: SET data receive: want=%zu, got=%zu, err=%d\n",
               to_receive, received, err);
        
        if (err == INFRA_ERROR_TIMEOUT || err == INFRA_ERROR_WOULD_BLOCK) {
            if (retry_count < max_retries) {
                printf("DEBUG: SET receive timeout/would block, retrying (%d/%d)\n", 
                       retry_count + 1, max_retries);
                retry_count++;
                usleep(10000); // 休眠10ms后重试
                continue;
            }
            printf("DEBUG: SET failed - timeout after %d retries\n", max_retries);
            free(data);
            if (!noreply) {
                infra_net_send(conn->sock, "CLIENT_ERROR timeout\r\n", 20, NULL);
            }
            setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &orig_tv, sizeof(orig_tv));
            setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &orig_rcvbuf, sizeof(orig_rcvbuf));
            fcntl(sock_fd, F_SETFL, flags_orig);  // 恢复原始flags
            conn->should_close = true;
            return;
        }
        
        if (err != INFRA_OK || received == 0) {
            if (err == INFRA_ERROR_CLOSED) {
                printf("DEBUG: SET failed - connection closed by peer\n");
            } else {
                printf("DEBUG: SET failed - error receiving data: %d\n", err);
            }
            free(data);
            if (!noreply) {
                infra_net_send(conn->sock, "CLIENT_ERROR bad data chunk\r\n", 28, NULL);
            }
            setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &orig_tv, sizeof(orig_tv));
            setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &orig_rcvbuf, sizeof(orig_rcvbuf));
            fcntl(sock_fd, F_SETFL, flags_orig);  // 恢复原始flags
            conn->should_close = true;
            return;
        }
        
        total_received += received;
        if (received > 0) {
            retry_count = 0;  // 只有在成功接收数据时才重置重试计数
        }
        
        // 打印进度
        if (bytes > chunk_size && total_received % (chunk_size * 8) == 0) {
            printf("DEBUG: SET progress: %zu/%zu bytes (%.1f%%)\n", 
                   total_received, bytes, (total_received * 100.0) / bytes);
        }
    }

    // 恢复原始设置
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &orig_tv, sizeof(orig_tv));
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &orig_rcvbuf, sizeof(orig_rcvbuf));
    fcntl(sock_fd, F_SETFL, flags_orig);  // 恢复原始flags

    char crlf[2];
    size_t received = 0;
    infra_error_t err = infra_net_recv(conn->sock, crlf, 2, &received);
    printf("DEBUG: SET reading CRLF: received=%zu, crlf=[%02x,%02x]\n",
           received, crlf[0], crlf[1]);
           
    if (err != INFRA_OK || received != 2 || crlf[0] != '\r' || crlf[1] != '\n') {
        printf("DEBUG: SET failed - bad CRLF\n");
        free(data);
        if (!noreply) {
            infra_net_send(conn->sock, "CLIENT_ERROR bad data chunk\r\n", 28, NULL);
        }
        conn->should_close = true;
        return;
    }

    if (exptime > 0 && exptime < 2592000) {
        exptime += time(NULL);
    }

    err = kv_set(conn->store, key, data, bytes, flags, exptime);
    printf("DEBUG: SET storage result: err=%d\n", err);
    free(data);

    if (!noreply) {
        if (err == INFRA_OK) {
            infra_net_send(conn->sock, "STORED\r\n", 8, NULL);
        } else {
            infra_net_send(conn->sock, "NOT_STORED\r\n", 12, NULL);
        }
    }
}

static void handle_delete(memkv_conn_t* conn, const char* key, bool noreply) {
    printf("DEBUG: Handling DELETE command for key='%s'\n", key);
    
    const char* sql = "DELETE FROM kv_store WHERE key = ?";
    poly_db_stmt_t* stmt = NULL;
    
    infra_error_t err = poly_db_prepare(conn->store, sql, &stmt);
    if (err != INFRA_OK) {
        if (!noreply) {
            infra_net_send(conn->sock, "ERROR\r\n", 7, NULL);
        }
        return;
    }
    
    err = poly_db_bind_text(stmt, 1, key, strlen(key));
    if (err != INFRA_OK) {
        poly_db_stmt_finalize(stmt);
        if (!noreply) {
            infra_net_send(conn->sock, "ERROR\r\n", 7, NULL);
        }
        return;
    }
    
    err = poly_db_stmt_step(stmt);
    poly_db_stmt_finalize(stmt);
    
    if (!noreply) {
        if (err == INFRA_OK) {
            infra_net_send(conn->sock, "DELETED\r\n", 9, NULL);
        } else {
            infra_net_send(conn->sock, "NOT_FOUND\r\n", 11, NULL);
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

// Forward declarations
static void handle_connection_wrapper(void* args);
static void handle_request_wrapper(void* args);

static void handle_request(poly_poll_handler_args_t* args) {
    if (!args) {
        INFRA_LOG_ERROR("Invalid connection args");
        return;
    }

    infra_socket_t client = args->client;
    memkv_conn_t* conn_state = (memkv_conn_t*)args->user_data;
    if (!conn_state) {
        INFRA_LOG_ERROR("Connection state not initialized");
        return;
    }

    // Get client address
    infra_net_addr_t addr;
    char client_addr[64];
    
    infra_error_t err = infra_net_get_peer_addr(client, &addr);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to get peer address: %d", err);
        return;
    }

    infra_net_addr_to_string(&addr, client_addr, sizeof(client_addr));
    INFRA_LOG_INFO("Processing request from %s", client_addr);

    // Process data
    size_t received = 0;
    err = infra_net_recv(conn_state->sock, conn_state->rx_buf + conn_state->rx_len, 
        MEMKV_BUFFER_SIZE - conn_state->rx_len - 1, &received);
    if (err != INFRA_OK) {
        if (err != INFRA_ERROR_TIMEOUT) {
            INFRA_LOG_ERROR("Failed to receive from %s: %d", client_addr, err);
            conn_state->should_close = true;
        }
        return;
    }
    
    if (received == 0) {
        INFRA_LOG_INFO("Client disconnected: %s", client_addr);
        conn_state->should_close = true;
        return;
    }

    conn_state->rx_len += received;
    conn_state->rx_buf[conn_state->rx_len] = '\0';

    // Process complete commands
    char* cmd_start = conn_state->rx_buf;
    char* cmd_end;
    while ((cmd_end = strstr(cmd_start, "\r\n")) != NULL) {
        *cmd_end = '\0';
        INFRA_LOG_DEBUG("Processing command from %s: %s", client_addr, cmd_start);

        // Parse command
        char* saveptr = NULL;
        char* cmd = strtok_r(cmd_start, " ", &saveptr);
        if (!cmd) {
            if (infra_net_send(conn_state->sock, "ERROR\r\n", 7, NULL) != INFRA_OK) {
                conn_state->should_close = true;
                return;
            }
            continue;
        }

        // Handle commands
        if (strcasecmp(cmd, "get") == 0) {
            char* key = strtok_r(NULL, " ", &saveptr);
            if (key) {
                handle_get(conn_state, key);
            } else {
                if (infra_net_send(conn_state->sock, "CLIENT_ERROR bad command line format\r\n", 37, NULL) != INFRA_OK) {
                    conn_state->should_close = true;
                    return;
                }
            }
        }
        else if (strcasecmp(cmd, "set") == 0) {
            char* key = strtok_r(NULL, " ", &saveptr);
            char* flags = strtok_r(NULL, " ", &saveptr);
            char* exptime = strtok_r(NULL, " ", &saveptr);
            char* bytes = strtok_r(NULL, " ", &saveptr);
            char* noreply = strtok_r(NULL, " ", &saveptr);
            if (key && flags && exptime && bytes) {
                handle_set(conn_state, key, flags, exptime, bytes, noreply && strcmp(noreply, "noreply") == 0);
            } else {
                if (infra_net_send(conn_state->sock, "CLIENT_ERROR bad command line format\r\n", 37, NULL) != INFRA_OK) {
                    conn_state->should_close = true;
                    return;
                }
            }
        }
        else if (strcasecmp(cmd, "delete") == 0) {
            char* key = strtok_r(NULL, " ", &saveptr);
            char* noreply = strtok_r(NULL, " ", &saveptr);
            if (key) {
                handle_delete(conn_state, key, noreply && strcmp(noreply, "noreply") == 0);
            } else {
                if (infra_net_send(conn_state->sock, "CLIENT_ERROR bad command line format\r\n", 37, NULL) != INFRA_OK) {
                    conn_state->should_close = true;
                    return;
                }
            }
        }
        else if (strcasecmp(cmd, "incr") == 0 || strcasecmp(cmd, "decr") == 0) {
            char* key = strtok_r(NULL, " ", &saveptr);
            char* value = strtok_r(NULL, " ", &saveptr);
            if (key && value) {
                handle_incr_decr(conn_state, key, value, strcasecmp(cmd, "incr") == 0);
            } else {
                if (infra_net_send(conn_state->sock, "CLIENT_ERROR bad command line format\r\n", 37, NULL) != INFRA_OK) {
                    conn_state->should_close = true;
                    return;
                }
            }
        }
        else if (strcasecmp(cmd, "flush_all") == 0) {
            char* delay = strtok_r(NULL, " ", &saveptr);  // 忽略延迟参数
            char* noreply = strtok_r(NULL, " ", &saveptr);
            handle_flush(conn_state, noreply && strcmp(noreply, "noreply") == 0);
        }
        else if (strcasecmp(cmd, "quit") == 0) {
            conn_state->should_close = true;
            break;
        }
        else {
            INFRA_LOG_DEBUG("Unknown command from %s: %s", client_addr, cmd);
            if (infra_net_send(conn_state->sock, "ERROR\r\n", 7, NULL) != INFRA_OK) {
                conn_state->should_close = true;
                return;
            }
        }

        // Move to next command
        cmd_start = cmd_end + 2;
        conn_state->rx_len -= (cmd_start - conn_state->rx_buf);
        if (conn_state->rx_len > 0) {
            memmove(conn_state->rx_buf, cmd_start, conn_state->rx_len);
        }
    }

    // Check if connection should be closed
    if (conn_state->should_close) {
        INFRA_LOG_INFO("Closing connection from %s", client_addr);
        if (conn_state->store) {
            poly_db_close(conn_state->store);
            conn_state->store = NULL;
        }
        if (conn_state->rx_buf) {
            infra_free(conn_state->rx_buf);
            conn_state->rx_buf = NULL;
        }
        infra_free(conn_state);
        args->user_data = NULL;
    }
}

static void handle_request_wrapper(void* args) {
    handle_request((poly_poll_handler_args_t*)args);
}

static void handle_connection(poly_poll_handler_args_t* args) {
    if (!args) {
        INFRA_LOG_ERROR("Invalid connection args");
        return;
    }

    infra_socket_t client = args->client;

    // Get client address
    infra_net_addr_t addr;
    char client_addr[64];
    
    infra_error_t err = infra_net_get_peer_addr(client, &addr);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to get peer address: %d", err);
        return;
    }

    infra_net_addr_to_string(&addr, client_addr, sizeof(client_addr));
    INFRA_LOG_INFO("New client connection from %s", client_addr);

    // Set socket to non-blocking mode
    err = infra_net_set_nonblock(client, true);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set non-blocking mode: %d", err);
        return;
    }

    // Initialize connection state
    memkv_conn_t* conn_state = (memkv_conn_t*)infra_malloc(sizeof(memkv_conn_t));
    if (!conn_state) {
        INFRA_LOG_ERROR("Failed to allocate connection state");
        return;
    }
    memset(conn_state, 0, sizeof(memkv_conn_t));
    
    conn_state->sock = client;
    conn_state->rx_buf = infra_malloc(MEMKV_BUFFER_SIZE);
    if (!conn_state->rx_buf) {
        INFRA_LOG_ERROR("Failed to allocate receive buffer");
        infra_free(conn_state);
        return;
    }
    conn_state->rx_len = 0;
    conn_state->should_close = false;

    // Initialize database connection
    err = db_init(&conn_state->store);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize database: %d", err);
        infra_free(conn_state->rx_buf);
        infra_free(conn_state);
        return;
    }

    // Set TCP_NODELAY to improve latency
    int sock_fd = (int)client;
    int flag = 1;
    if (setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
        INFRA_LOG_ERROR("Failed to set TCP_NODELAY");
    }

    // Set connection state in args
    args->user_data = conn_state;
}

static void handle_connection_wrapper(void* args) {
    handle_connection((poly_poll_handler_args_t*)args);
}

//-----------------------------------------------------------------------------
// Service Interface Implementation
//-----------------------------------------------------------------------------

infra_error_t memkv_init(void) {
    if (g_memkv_service.state != PEER_SERVICE_STATE_INIT &&
        g_memkv_service.state != PEER_SERVICE_STATE_STOPPED) {
        return INFRA_ERROR_INVALID_STATE;
    }
    
    // Set default values
    g_memkv_state.port = MEMKV_DEFAULT_PORT;
    g_memkv_state.engine = "sqlite";  // Default to SQLite
    g_memkv_state.plugin = NULL;
    g_memkv_state.running = false;
    g_memkv_state.poll_ctx = NULL;

    g_memkv_service.state = PEER_SERVICE_STATE_READY;
    return INFRA_OK;
}

infra_error_t memkv_cleanup(void) {
    if (g_memkv_service.state == PEER_SERVICE_STATE_RUNNING) {
        INFRA_LOG_ERROR("Cannot cleanup while service is running");
        return INFRA_ERROR_INVALID_STATE;
    }

    // Stop service if running
    if (g_memkv_state.running) {
        memkv_stop();
    }

    // Reset all state
    g_memkv_state.port = 0;
    g_memkv_state.running = false;
    g_memkv_state.poll_ctx = NULL;

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

    // 如果状态是 INIT，先尝试初始化
    if (g_memkv_service.state == PEER_SERVICE_STATE_INIT) {
        err = memkv_init();
        if (err != INFRA_OK) {
            return err;
        }
    }

    if (g_memkv_service.state != PEER_SERVICE_STATE_READY &&
        g_memkv_service.state != PEER_SERVICE_STATE_STOPPED) {
        INFRA_LOG_ERROR("Service is in invalid state: %d", g_memkv_service.state);
        return INFRA_ERROR_INVALID_STATE;
    }

    if (g_memkv_state.running) {
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    // 确保端口已设置，如果没有设置则使用默认端口
    if (g_memkv_state.port <= 0) {
        g_memkv_state.port = MEMKV_DEFAULT_PORT;
    }

    INFRA_LOG_INFO("Initializing MemKV service with port=%d", g_memkv_state.port);

    // Create poll context
    poly_poll_config_t poll_config = {
        .min_threads = 1,
        .max_threads = 4,
        .queue_size = 1000,
        .max_listeners = 1
    };

    g_memkv_state.poll_ctx = (poly_poll_context_t*)infra_malloc(sizeof(poly_poll_context_t));
    if (!g_memkv_state.poll_ctx) {
        INFRA_LOG_ERROR("Failed to allocate poll context");
        g_memkv_state.poll_ctx = NULL;
        g_memkv_service.state = PEER_SERVICE_STATE_STOPPED;
        return INFRA_ERROR_NO_MEMORY;
    }

    err = poly_poll_init(g_memkv_state.poll_ctx, &poll_config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create poll context: %d", err);
        infra_free(g_memkv_state.poll_ctx);
        g_memkv_state.poll_ctx = NULL;
        g_memkv_service.state = PEER_SERVICE_STATE_STOPPED;
        return err;
    }

    // Set request handler
    poly_poll_set_handler(g_memkv_state.poll_ctx, handle_connection_wrapper);

    // Add listener
    poly_poll_listener_t listener_config = {0};  // 清零所有字段
    listener_config.bind_port = g_memkv_state.port;
    listener_config.user_data = NULL;
    strcpy(listener_config.bind_addr, "0.0.0.0");//TODO user host from config

    INFRA_LOG_INFO("Adding listener on %s:%d", listener_config.bind_addr, listener_config.bind_port);

    err = poly_poll_add_listener(g_memkv_state.poll_ctx, &listener_config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to add listener: %d", err);
        poly_poll_cleanup(g_memkv_state.poll_ctx);
        infra_free(g_memkv_state.poll_ctx);
        g_memkv_state.poll_ctx = NULL;
        g_memkv_service.state = PEER_SERVICE_STATE_STOPPED;
        return err;
    }

    // Start polling in a new thread
    g_memkv_state.running = true;
    infra_thread_t thread;
    err = infra_thread_create(&thread, (infra_thread_func_t)poly_poll_start, g_memkv_state.poll_ctx);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create polling thread: %d", err);
        g_memkv_state.running = false;
        poly_poll_cleanup(g_memkv_state.poll_ctx);
        infra_free(g_memkv_state.poll_ctx);
        g_memkv_state.poll_ctx = NULL;
        g_memkv_service.state = PEER_SERVICE_STATE_STOPPED;
        return err;
    }

    // 等待服务启动
    infra_sleep(100);  // 等待100ms让服务启动

    INFRA_LOG_INFO("MemKV service started successfully on port %d", g_memkv_state.port);

    g_memkv_service.state = PEER_SERVICE_STATE_RUNNING;
    return INFRA_OK;
}

infra_error_t memkv_stop(void) {
    if (g_memkv_service.state != PEER_SERVICE_STATE_RUNNING) {
        INFRA_LOG_ERROR("Service is not running");
        return INFRA_ERROR_INVALID_STATE;
    }

    if (!g_memkv_state.running) {
        return INFRA_OK;
    }

    // Stop the service
    g_memkv_state.running = false;

    // Cleanup poll context
    if (g_memkv_state.poll_ctx) {
        poly_poll_cleanup(g_memkv_state.poll_ctx);
        infra_free(g_memkv_state.poll_ctx);
        g_memkv_state.poll_ctx = NULL;
    }

    // Free engine and plugin strings if allocated
    if (g_memkv_state.engine) {
        free(g_memkv_state.engine);
        g_memkv_state.engine = NULL;
    }
    if (g_memkv_state.plugin) {
        free(g_memkv_state.plugin);
        g_memkv_state.plugin = NULL;
    }

    g_memkv_service.state = PEER_SERVICE_STATE_STOPPED;
    return INFRA_OK;
}

infra_error_t memkv_cmd_handler(const char* cmd, char* response, size_t size) {
    if (!cmd || !response || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 解析命令参数
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

    // 处理命令
    if (argc == 0) {
        snprintf(response, size, "Error: Empty command");
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (strcmp(argv[0], "status") == 0) {
        const char* state_str = "unknown";
        switch (g_memkv_service.state) {
            case PEER_SERVICE_STATE_INIT:
                state_str = "initialized";
                break;
            case PEER_SERVICE_STATE_READY:
                state_str = "ready";
                break;
            case PEER_SERVICE_STATE_RUNNING:
                state_str = "running";
                break;
            case PEER_SERVICE_STATE_STOPPED:
                state_str = "stopped";
                break;
        }
        snprintf(response, size, "MemKV Service Status:\n"
                "State: %s\n"
                "Port: %d\n"
                "Engine: %s\n"
                "Plugin: %s\n",
                state_str,
                g_memkv_state.port,
                g_memkv_state.engine ? g_memkv_state.engine : "none",
                g_memkv_state.plugin ? g_memkv_state.plugin : "none");
        return INFRA_OK;
    }
    else if (strcmp(argv[0], "start") == 0) {
        // 解析配置参数
        const char* config_file = NULL;
        for (int i = 1; i < argc; i++) {
            if (strncmp(argv[i], "--config=", 9) == 0) {
                config_file = argv[i] + 9;
                FILE* fp = fopen(config_file, "r");
                if (fp) {
                    char line[1024];
                    if (fgets(line, sizeof(line), fp)) {
                        char host[256];
                        int port;
                        char engine[64];
                        char plugin[256];
                        
                        // 尝试解析配置行
                        int matched = sscanf(line, "%255s %d %63s %255s", host, &port, engine, plugin);
                        INFRA_LOG_INFO("Parsing config file: matched=%d, host=%s, port=%d, engine=%s", 
                                      matched, host, port, engine);
                        
                        if (matched >= 3) {
                            g_memkv_state.port = port;
                            if (g_memkv_state.engine) free(g_memkv_state.engine);
                            g_memkv_state.engine = strdup(engine);
                            if (matched > 3 && plugin[0] != '#') {  // 如果不是注释
                                if (g_memkv_state.plugin) free(g_memkv_state.plugin);
                                g_memkv_state.plugin = strdup(plugin);
                            }
                            INFRA_LOG_INFO("Config loaded: port=%d, engine=%s, plugin=%s",
                                          g_memkv_state.port, g_memkv_state.engine,
                                          g_memkv_state.plugin ? g_memkv_state.plugin : "none");
                        } else {
                            INFRA_LOG_ERROR("Failed to parse config line: %s", line);
                        }
                    }
                    fclose(fp);
                } else {
                    INFRA_LOG_ERROR("Failed to open config file: %s", config_file);
                }
                break;
            }
            else if (strncmp(argv[i], "--port=", 7) == 0) {
                g_memkv_state.port = atoi(argv[i] + 7);
            }
            else if (strncmp(argv[i], "--engine=", 9) == 0) {
                g_memkv_state.engine = strdup(argv[i] + 9);
            }
            else if (strncmp(argv[i], "--plugin=", 9) == 0) {
                g_memkv_state.plugin = strdup(argv[i] + 9);
            }
        }

        // 启动服务
        infra_error_t err = memkv_start();
        if (err != INFRA_OK) {
            snprintf(response, size, "Failed to start MemKV service: %d\n", err);
            return err;
        }

        snprintf(response, size, "MemKV service started\n");
        return INFRA_OK;
    }
    else if (strcmp(argv[0], "stop") == 0) {
        // 停止服务
        infra_error_t err = memkv_stop();
        if (err != INFRA_OK) {
            snprintf(response, size, "Failed to stop MemKV service: %d\n", err);
            return err;
        }

        snprintf(response, size, "MemKV service stopped\n");
        return INFRA_OK;
    }

    snprintf(response, size, "Unknown command: %s", argv[0]);
    return INFRA_ERROR_NOT_FOUND;
}

// Get memkv service instance
peer_service_t* peer_memkv_get_service(void) {
    return &g_memkv_service;
}
