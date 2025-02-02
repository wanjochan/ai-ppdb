#include "peer_service.h"
#include "../infra/infra_core.h"
#include "../infra/infra_net.h"
#include "../infra/infra_sync.h"
#include "../infra/infra_memory.h"
#include "../infra/infra_error.h"
#include "../poly/poly_db.h"
#include "../poly/poly_poll.h"
#include <netinet/tcp.h>  // 添加TCP_NODELAY的定义

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
    if (!db) return INFRA_ERROR_INVALID_PARAM;
    
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
    int sock_fd = infra_net_get_fd(conn->sock);
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

static void handle_connection(void* arg) {
    if (!arg) {
        printf("DEBUG: Invalid connection arguments\n");
        return;
    }

    poly_poll_handler_args_t* args = (poly_poll_handler_args_t*)arg;
    if (!args->client) {
        printf("DEBUG: Invalid client socket\n");
        free(args);
        return;
    }

    printf("DEBUG: New connection accepted\n");
    
    // 初始化连接结构
    memkv_conn_t conn = {0};  // 初始化所有字段为0
    conn.sock = args->client;
    
    // 设置socket配置
    int sock_fd = infra_net_get_fd(conn.sock);
    if (sock_fd < 0) {
        printf("DEBUG: Invalid socket fd\n");
        goto cleanup;
    }

    // 设置超时
    struct timeval tv;
    tv.tv_sec = 300;  // 5分钟超时
    tv.tv_usec = 0;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0) {
        printf("DEBUG: Failed to set SO_RCVTIMEO\n");
    }
    if (setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv) < 0) {
        printf("DEBUG: Failed to set SO_SNDTIMEO\n");
    }

    // 设置缓冲区
    int buf_size = MEMKV_BUFFER_SIZE;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(buf_size)) < 0) {
        printf("DEBUG: Failed to set SO_RCVBUF to %d\n", buf_size);
    }
    if (setsockopt(sock_fd, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size)) < 0) {
        printf("DEBUG: Failed to set SO_SNDBUF to %d\n", buf_size);
    }

    // 禁用Nagle算法
    int flag = 1;
    if (setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
        printf("DEBUG: Failed to set TCP_NODELAY\n");
    }

    // 启用TCP保活
    if (setsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag)) < 0) {
        printf("DEBUG: Failed to set SO_KEEPALIVE\n");
    }

    // 设置TCP保活参数
#ifdef TCP_KEEPIDLE
    int keepalive_time = 60;  // 60秒无数据时发送保活包
    if (setsockopt(sock_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepalive_time, sizeof(keepalive_time)) < 0) {
        printf("DEBUG: Failed to set TCP_KEEPIDLE\n");
    }
#endif

#ifdef TCP_KEEPINTVL
    int keepalive_intvl = 10;  // 每10秒发送一次保活包
    if (setsockopt(sock_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepalive_intvl, sizeof(keepalive_intvl)) < 0) {
        printf("DEBUG: Failed to set TCP_KEEPINTVL\n");
    }
#endif

#ifdef TCP_KEEPCNT
    int keepalive_probes = 6;  // 最多发送6次保活包
    if (setsockopt(sock_fd, IPPROTO_TCP, TCP_KEEPCNT, &keepalive_probes, sizeof(keepalive_probes)) < 0) {
        printf("DEBUG: Failed to set TCP_KEEPCNT\n");
    }
#endif

    // 分配接收缓冲区
    conn.rx_buf = malloc(MEMKV_BUFFER_SIZE);
    if (!conn.rx_buf) {
        printf("DEBUG: Failed to allocate rx buffer\n");
        goto cleanup;
    }
    
    // 初始化数据库
    infra_error_t err = db_init(&conn.store);
    if (err != INFRA_OK) {
        printf("DEBUG: Failed to initialize database: err=%d\n", err);
        goto cleanup;
    }

    printf("DEBUG: Connection setup complete, entering command loop\n");

    // 命令处理循环
    while (!conn.should_close) {
        char cmd_line[1024];  // 使用较小的命令行缓冲区
        size_t cmd_len = 0;
        bool has_data = false;

        // 读取命令行
        while (cmd_len < sizeof(cmd_line) - 1) {
            size_t received = 0;
            err = infra_net_recv(conn.sock, cmd_line + cmd_len, 1, &received);
            
            if (err != INFRA_OK) {
                if (err == INFRA_ERROR_TIMEOUT) {
                    if (!has_data) {
                        continue;  // 如果还没收到任何数据，继续等待
                    }
                    printf("DEBUG: Command read timeout with partial data\n");
                    goto cleanup;
                }
                if (err == INFRA_ERROR_CLOSED) {
                    printf("DEBUG: Connection closed by peer\n");
                    goto cleanup;
                }
                printf("DEBUG: Command read error: %d\n", err);
                goto cleanup;
            }
            
            if (received == 0) {
                printf("DEBUG: Connection closed by peer\n");
                goto cleanup;
            }
            
            has_data = true;
            cmd_len++;
            
            if (cmd_len >= 2 && cmd_line[cmd_len-2] == '\r' && cmd_line[cmd_len-1] == '\n') {
                cmd_line[cmd_len-2] = '\0';
                printf("DEBUG: Received command: [%s]\n", cmd_line);
                break;
            }
        }

        if (cmd_len >= sizeof(cmd_line) - 1) {
            printf("DEBUG: Command too long\n");
            if (infra_net_send(conn.sock, "CLIENT_ERROR line too long\r\n", 26, NULL) != INFRA_OK) {
                goto cleanup;
            }
            continue;
        }

        if (cmd_len == 0) {
            printf("DEBUG: Empty command received\n");
            continue;
        }

        char* saveptr = NULL;
        char* cmd = strtok_r(cmd_line, " ", &saveptr);
        if (!cmd) {
            printf("DEBUG: Invalid command format\n");
            continue;
        }

        printf("DEBUG: Processing command: %s\n", cmd);

        if (strcasecmp(cmd, "quit") == 0) {
            printf("DEBUG: Client requested quit\n");
            goto cleanup;
        }
        
        // 处理其他命令...
        if (strcasecmp(cmd, "get") == 0) {
            char* key = strtok_r(NULL, " ", &saveptr);
            if (key) {
                handle_get(&conn, key);
            } else {
                if (infra_net_send(conn.sock, "CLIENT_ERROR no key\r\n", 20, NULL) != INFRA_OK) {
                    goto cleanup;
                }
            }
        }
        else if (strcasecmp(cmd, "set") == 0) {
            char* key = strtok_r(NULL, " ", &saveptr);
            char* flags = strtok_r(NULL, " ", &saveptr);
            char* exptime = strtok_r(NULL, " ", &saveptr);
            char* bytes_str = strtok_r(NULL, " ", &saveptr);
            char* noreply = strtok_r(NULL, " ", &saveptr);

            if (!key || !flags || !exptime || !bytes_str) {
                printf("DEBUG: SET command missing parameters\n");
                if (infra_net_send(conn.sock, "CLIENT_ERROR bad command line format\r\n", 37, NULL) != INFRA_OK) {
                    goto cleanup;
                }
                continue;
            }

            handle_set(&conn, key, flags, exptime, bytes_str, noreply && strcmp(noreply, "noreply") == 0);
            if (conn.should_close) goto cleanup;
        }
        else if (strcasecmp(cmd, "delete") == 0) {
            char* key = strtok_r(NULL, " ", &saveptr);
            char* noreply = strtok_r(NULL, " ", &saveptr);
            if (key) {
                handle_delete(&conn, key, noreply && strcmp(noreply, "noreply") == 0);
            } else {
                if (infra_net_send(conn.sock, "CLIENT_ERROR no key\r\n", 20, NULL) != INFRA_OK) {
                    goto cleanup;
                }
            }
        }
        else if (strcasecmp(cmd, "incr") == 0 || strcasecmp(cmd, "decr") == 0) {
            char* key = strtok_r(NULL, " ", &saveptr);
            char* value = strtok_r(NULL, " ", &saveptr);
            if (key && value) {
                handle_incr_decr(&conn, key, value, strcasecmp(cmd, "incr") == 0);
                if (conn.should_close) goto cleanup;
            } else {
                if (infra_net_send(conn.sock, "CLIENT_ERROR bad command line format\r\n", 37, NULL) != INFRA_OK) {
                    goto cleanup;
                }
            }
        }
        else if (strcasecmp(cmd, "flush_all") == 0) {
            char* delay = strtok_r(NULL, " ", &saveptr);  // 忽略延迟参数
            char* noreply = strtok_r(NULL, " ", &saveptr);
            handle_flush(&conn, noreply && strcmp(noreply, "noreply") == 0);
        }
        else {
            printf("DEBUG: Unknown command: %s\n", cmd);
            if (infra_net_send(conn.sock, "ERROR\r\n", 7, NULL) != INFRA_OK) {
                goto cleanup;
            }
        }
    }

cleanup:
    printf("DEBUG: Cleaning up connection\n");
    if (conn.store) {
        poly_db_close(conn.store);
        conn.store = NULL;
    }
    if (conn.rx_buf) {
        free(conn.rx_buf);
        conn.rx_buf = NULL;
    }
    if (conn.sock) {
        infra_net_close(conn.sock);
        conn.sock = NULL;
    }
    if (args) {
        free(args);
        args = NULL;
    }
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
