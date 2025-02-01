#include "peer_service.h"
#include "../infra/infra_core.h"
#include "../infra/infra_net.h"
#include "../infra/infra_sync.h"
#include "../infra/infra_memory.h"
#include "../poly/poly_memkv.h"
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
#define MEMKV_MAX_ADDR_LEN 256
#define MEMKV_MAX_RULES 16
#define RING_BUFFER_SIZE 8192  // 增加到 8KB
#define MEMKV_BUFFER_SIZE 1048576  // 1MB
#define MEMKV_MAX_KEY_SIZE 250
#define MEMKV_MAX_VALUE_SIZE (1024 * 1024)  // 1MB
#define MEMKV_MIN_THREADS 32
#define MEMKV_MAX_THREADS 512
#define MEMKV_DEFAULT_PORT 11211
#define POLY_OK 0
#define POLY_ERR_NOT_FOUND 1

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
    char* data;
    size_t bytes_available;
    size_t read_pos;
    size_t write_pos;
} ring_buffer_t;

typedef struct {
    infra_socket_t sock;        // 客户端套接字
    bool binary_protocol;       // 是否使用二进制协议
    bool should_close;          // 是否应该关闭连接
    poly_memkv_db_t* store;    // 存储引擎实例
    ring_buffer_t rx_buf;      // 接收缓冲区
    ring_buffer_t tx_buf;      // 发送缓冲区
    char* cmd_buf;             // 命令累积缓冲区
    size_t cmd_len;            // 当前命令长度
} memkv_conn_t;

typedef struct {
    char addr[MEMKV_MAX_ADDR_LEN];
    int port;
    bool binary_protocol;
} memkv_rule_t;

typedef struct {
    memkv_rule_t rules[MEMKV_MAX_RULES];
    int rule_count;
    int max_threads;
    int min_threads;
    char* engine;
    char* plugin;
    bool running;
    poly_poll_context_t* poll_ctx;  // Fix type name
} memkv_config_t;

typedef struct {
    poly_memkv_db_t* db;
    poly_memkv_iter_t* iter;
} peer_memkv_iter_t;

typedef struct {
    time_t expiry;
    uint32_t flags;
    size_t value_len;
} memkv_item_t;

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
        .config = NULL,
        .config_path = NULL
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
// Additional Forward Declarations
//-----------------------------------------------------------------------------

static void handle_flush_all_command(memkv_conn_t* conn);
static void handle_get_command(memkv_conn_t* conn, const char* key);
static void handle_set_command(memkv_conn_t* conn, const char* key, const char* value, size_t value_len);
static void handle_text_command(memkv_conn_t* conn, const char* cmd);
static void handle_binary_command(memkv_conn_t* conn, const char* cmd, size_t cmd_len);
static void handle_connection(void* arg);
static infra_error_t load_config(const char* config_path);
static bool is_key_expired(poly_memkv_db_t* db, const char* key);
static infra_error_t set_with_expiry(poly_memkv_db_t* db, const char* key, const void* value, 
                                    size_t value_len, uint32_t flags, time_t expiry);
static infra_error_t get_with_expiry(poly_memkv_db_t* db, const char* key, void** value, 
                                    size_t* value_len, uint32_t* flags);
static infra_error_t poly_memkv_counter_op(poly_memkv_db_t* db, const char* key, int64_t delta, int64_t* new_value);
static infra_error_t poly_memkv_incr(poly_memkv_db_t* db, const char* key, int64_t delta, int64_t* new_value);
static infra_error_t poly_memkv_decr(poly_memkv_db_t* db, const char* key, int64_t delta, int64_t* new_value);
static bool send_response(infra_socket_t sock, const char* response, size_t len);
static void handle_request(memkv_conn_t* conn, char* request, size_t len);
static void handle_client_command(memkv_conn_t* conn, const char* cmd);
static void send_binary_error_response(memkv_conn_t* conn, const char* error_msg);
static void handle_incr_command(memkv_conn_t* conn, const char* key, const char* value);
memkv_conn_t* memkv_conn_create(infra_socket_t sock);
static void memkv_conn_destroy(memkv_conn_t* conn);

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

// 环形缓冲区操作
static void ring_buffer_init(ring_buffer_t* buf) {
    if (!buf) return;
    
    buf->data = (char*)malloc(RING_BUFFER_SIZE);
    if (!buf->data) {
        INFRA_LOG_ERROR("Failed to allocate ring buffer memory");
        return;
    }
    
    buf->bytes_available = 0;
    buf->read_pos = 0;
    buf->write_pos = 0;
}

static void ring_buffer_cleanup(ring_buffer_t* buf) {
    if (buf && buf->data) {
        free(buf->data);
        buf->data = NULL;
        buf->bytes_available = 0;
        buf->read_pos = 0;
        buf->write_pos = 0;
    }
}

static size_t ring_buffer_write(ring_buffer_t* buf, const char* data, size_t len) {
    if (!buf || !buf->data || !data || len == 0) {
        return 0;
    }
    
    // 计算可用空间
    size_t free_space = RING_BUFFER_SIZE - buf->bytes_available;
    if (free_space == 0) {
        INFRA_LOG_ERROR("Ring buffer is full");
        return 0;  // 缓冲区已满
    }
    
    // 调整写入长度
    len = (len > free_space) ? free_space : len;
    
    // 计算到缓冲区末尾的空间
    size_t to_end = RING_BUFFER_SIZE - buf->write_pos;
    
    if (to_end >= len) {
        // 可以一次性写入
        memcpy(buf->data + buf->write_pos, data, len);
        buf->write_pos = (buf->write_pos + len) % RING_BUFFER_SIZE;
    } else {
        // 需要分两次写入
        memcpy(buf->data + buf->write_pos, data, to_end);
        memcpy(buf->data, data + to_end, len - to_end);
        buf->write_pos = len - to_end;
    }
    
    buf->bytes_available += len;
    INFRA_LOG_DEBUG("Ring buffer write: len=%zu, available=%zu, read_pos=%zu, write_pos=%zu",
                    len, buf->bytes_available, buf->read_pos, buf->write_pos);
    return len;
}

static size_t ring_buffer_read(ring_buffer_t* buf, char* data, size_t len) {
    if (!buf || !buf->data || !data || len == 0 || buf->bytes_available == 0) {
        return 0;
    }
    
    // 调整读取长度
    len = (len > buf->bytes_available) ? buf->bytes_available : len;
    
    // 计算到缓冲区末尾的数据量
    size_t to_end = RING_BUFFER_SIZE - buf->read_pos;
    
    if (to_end >= len) {
        // 可以一次性读取
        memcpy(data, buf->data + buf->read_pos, len);
        buf->read_pos = (buf->read_pos + len) % RING_BUFFER_SIZE;
    } else {
        // 需要分两次读取
        memcpy(data, buf->data + buf->read_pos, to_end);
        memcpy(data + to_end, buf->data, len - to_end);
        buf->read_pos = len - to_end;
    }
    
    buf->bytes_available -= len;
    INFRA_LOG_DEBUG("Ring buffer read: len=%zu, available=%zu, read_pos=%zu, write_pos=%zu",
                    len, buf->bytes_available, buf->read_pos, buf->write_pos);
    return len;
}

static void ring_buffer_clear(ring_buffer_t* buf) {
    if (buf) {
        buf->bytes_available = 0;
        buf->read_pos = 0;
        buf->write_pos = 0;
    }
}

// 处理客户端连接
static void handle_connection(void* arg) {
    if (!arg) {
        INFRA_LOG_ERROR("Null argument passed to handle_connection");
        return;
    }

    poly_poll_handler_args_t* args = (poly_poll_handler_args_t*)arg;
    
    // 获取客户端地址
    struct sockaddr_in peer_addr;
    socklen_t addr_len = sizeof(peer_addr);
    char client_addr[256] = {0};
    
    if (getpeername(infra_net_get_fd(args->client), (struct sockaddr*)&peer_addr, &addr_len) == 0) {
        inet_ntop(AF_INET, &peer_addr.sin_addr, client_addr, sizeof(client_addr));
        INFRA_LOG_DEBUG("New connection received from %s:%d", client_addr, ntohs(peer_addr.sin_port));
    } else {
        INFRA_LOG_DEBUG("New connection received (failed to get peer address)");
    }
    
    memkv_conn_t* conn = memkv_conn_create(args->client);
    if (!conn) {
        INFRA_LOG_ERROR("Failed to create connection context");
        infra_net_close(args->client);
        free(args);
        return;
    }

    // Debug log connection details
    INFRA_LOG_DEBUG("New connection established on socket fd=%d", infra_net_get_fd(args->client));
    
    // 分配接收缓冲区
    char* rx_buffer = malloc(RING_BUFFER_SIZE);
    if (!rx_buffer) {
        INFRA_LOG_ERROR("Failed to allocate receive buffer");
        memkv_conn_destroy(conn);
        infra_net_close(args->client);
        free(args);
        return;
    }

    // 主处理循环
    while (!conn->should_close) {
        // 1. 检查并发送响应
        if (conn->tx_buf.bytes_available > 0) {
            size_t to_send = conn->tx_buf.bytes_available;
            size_t sent = 0;
            
            // 计算要发送的数据
            size_t first_chunk = RING_BUFFER_SIZE - conn->tx_buf.read_pos;
            if (first_chunk >= to_send) {
                // 数据是连续的
                infra_error_t err = infra_net_send(conn->sock, 
                                                 conn->tx_buf.data + conn->tx_buf.read_pos, 
                                                 to_send, &sent);
                if (err != INFRA_OK) {
                    INFRA_LOG_ERROR("Failed to send response: %d", err);
                    break;
                }
            } else {
                // 数据跨越了缓冲区边界，需要两次发送
                infra_error_t err = infra_net_send(conn->sock, 
                                                 conn->tx_buf.data + conn->tx_buf.read_pos,
                                                 first_chunk, &sent);
                if (err != INFRA_OK) {
                    INFRA_LOG_ERROR("Failed to send first chunk: %d", err);
                    break;
                }
                
                size_t remaining = to_send - first_chunk;
                size_t sent2 = 0;
                err = infra_net_send(conn->sock, conn->tx_buf.data, remaining, &sent2);
                if (err != INFRA_OK) {
                    INFRA_LOG_ERROR("Failed to send second chunk: %d", err);
                    break;
                }
                sent += sent2;
            }
            
            // 更新缓冲区状态
            conn->tx_buf.read_pos = (conn->tx_buf.read_pos + sent) % RING_BUFFER_SIZE;
            conn->tx_buf.bytes_available -= sent;
            
            INFRA_LOG_DEBUG("Sent %zu bytes of response", sent);
        }

        // 2. 接收并处理请求
        INFRA_LOG_DEBUG("Waiting for client data...");
        size_t received = 0;
        infra_error_t err = infra_net_recv(conn->sock, rx_buffer, RING_BUFFER_SIZE - conn->rx_buf.bytes_available, &received);
        
        if (err == INFRA_ERROR_TIMEOUT) {
            INFRA_LOG_DEBUG("Connection timed out");
            continue;
        }
        
        if (err != INFRA_OK) {
            if (err == INFRA_ERROR_CLOSED) {
                INFRA_LOG_DEBUG("Connection closed by peer");
            } else {
                INFRA_LOG_ERROR("Failed to receive data: %d", err);
            }
            break;
        }

        if (received > 0) {
            INFRA_LOG_DEBUG("Received %zu bytes from client", received);
            
            // 将数据写入接收缓冲区
            size_t written = ring_buffer_write(&conn->rx_buf, rx_buffer, received);
            if (written != received) {
                INFRA_LOG_ERROR("Failed to write to receive buffer: written=%zu, received=%zu", 
                               written, received);
                // 不要立即退出，尝试处理已经写入的数据
            }
            
            // 处理请求
            if (written > 0) {
                handle_request(conn, rx_buffer, written);
            }
        }
    }

    // 清理
    INFRA_LOG_DEBUG("Cleaning up connection");
    free(rx_buffer);
    memkv_conn_destroy(conn);
    infra_net_close(args->client);
    free(args);
}

static void handle_request(memkv_conn_t* conn, char* request, size_t len) {
    if (!conn || !request || len == 0) {
        INFRA_LOG_ERROR("Invalid request parameters");
        return;
    }

    // Debug log incoming request
    INFRA_LOG_DEBUG("Received request: len=%zu", len);
    
    // 处理命令
    if ((unsigned char)request[0] == 0x80) {
        INFRA_LOG_DEBUG("Processing binary protocol command");
        handle_binary_command(conn, request, len);
    } else {
        // 为文本命令创建一个临时缓冲区，确保以 null 结尾
        char* cmd_buf = (char*)malloc(len + 1);
        if (!cmd_buf) {
            INFRA_LOG_ERROR("Failed to allocate command buffer");
            ring_buffer_write(&conn->tx_buf, "SERVER_ERROR out of memory\r\n", 27);
            return;
        }
        
        // 复制命令并确保以 null 结尾
        memcpy(cmd_buf, request, len);
        cmd_buf[len] = '\0';
        
        // 查找并处理命令结束符 (\r\n)
        char* cmd_end = strstr(cmd_buf, "\r\n");
        if (!cmd_end) {
            INFRA_LOG_ERROR("Invalid command format - missing \\r\\n");
            ring_buffer_write(&conn->tx_buf, "CLIENT_ERROR invalid command format\r\n", 36);
            free(cmd_buf);
            return;
        }
        
        // 计算命令长度（不包括\r\n）
        size_t cmd_len = cmd_end - cmd_buf;
        
        // 将命令写入接收缓冲区
        size_t written = ring_buffer_write(&conn->rx_buf, request, len);
        if (written != len) {
            INFRA_LOG_ERROR("Failed to write request to buffer");
            ring_buffer_write(&conn->tx_buf, "SERVER_ERROR buffer full\r\n", 25);
            free(cmd_buf);
            return;
        }
        
        // 处理命令
        *cmd_end = '\0';  // 在 \r\n 处截断命令
        INFRA_LOG_DEBUG("Processing text protocol command: %s", cmd_buf);
        handle_text_command(conn, cmd_buf);
        free(cmd_buf);
    }
}

static void handle_text_command(memkv_conn_t* conn, const char* cmd) {
    if (!conn || !cmd) {
        return;
    }

    char* cmd_copy = strdup(cmd);
    if (!cmd_copy) {
        ring_buffer_write(&conn->tx_buf, "SERVER_ERROR out of memory\r\n", 27);
        return;
    }

    char* saveptr = NULL;
    char* token = strtok_r(cmd_copy, " \r\n", &saveptr);
    if (!token) {
        free(cmd_copy);
        ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
        return;
    }

    if (strcasecmp(token, "get") == 0) {
        token = strtok_r(NULL, " \r\n", &saveptr);
        if (token) {
            handle_get_command(conn, token);
        } else {
            ring_buffer_write(&conn->tx_buf, "CLIENT_ERROR no key\r\n", 20);
        }
    } else if (strcasecmp(token, "set") == 0) {
        char* key = strtok_r(NULL, " \r\n", &saveptr);
        char* flags_str = strtok_r(NULL, " \r\n", &saveptr);
        char* exptime_str = strtok_r(NULL, " \r\n", &saveptr);
        char* bytes_str = strtok_r(NULL, " \r\n", &saveptr);
        
        if (!key || !flags_str || !exptime_str || !bytes_str) {
            ring_buffer_write(&conn->tx_buf, "CLIENT_ERROR bad command line format\r\n", 37);
            free(cmd_copy);
            return;
        }

        size_t bytes = strtoul(bytes_str, NULL, 10);
        time_t exptime = strtol(exptime_str, NULL, 10);
        uint32_t flags = strtoul(flags_str, NULL, 10);

        // 查找数据部分
        char* data_start = strchr(cmd + (key - cmd_copy), '\r');
        if (!data_start || data_start[1] != '\n' || strlen(data_start) < bytes + 4) {
            ring_buffer_write(&conn->tx_buf, "CLIENT_ERROR bad data chunk\r\n", 28);
            free(cmd_copy);
            return;
        }
        data_start += 2;  // 跳过 \r\n

        // 设置过期时间
        if (exptime > 0) {
            if (exptime < 2592000) {  // 30天
                exptime += time(NULL);
            }
        }

        infra_error_t err = set_with_expiry(conn->store, key, data_start, bytes, flags, exptime);
        if (err == INFRA_OK) {
            ring_buffer_write(&conn->tx_buf, "STORED\r\n", 8);
        } else {
            ring_buffer_write(&conn->tx_buf, "NOT_STORED\r\n", 12);
        }
    } else if (strcasecmp(token, "incr") == 0) {
        char* key = strtok_r(NULL, " \r\n", &saveptr);
        char* value = strtok_r(NULL, " \r\n", &saveptr);
        if (key && value) {
            handle_incr_command(conn, key, value);
        } else {
            ring_buffer_write(&conn->tx_buf, "CLIENT_ERROR bad command line format\r\n", 37);
        }
    } else if (strcasecmp(token, "flush_all") == 0) {
        handle_flush_all_command(conn);
    } else {
        ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
    }

    free(cmd_copy);
}

static void handle_get_command(memkv_conn_t* conn, const char* key) {
    if (!conn || !key) {
        INFRA_LOG_ERROR("Invalid parameters for GET command");
        return;
    }

    void* value = NULL;
    size_t value_len = 0;
    uint32_t flags = 0;

    infra_error_t err = get_with_expiry(conn->store, key, &value, &value_len, &flags);
    if (err == INFRA_OK && value) {
        // 构造响应
        char header[128];
        int header_len = snprintf(header, sizeof(header), "VALUE %s %u %zu\r\n", 
                                key, flags, value_len);
        
        // 发送响应头
        if (header_len > 0 && header_len < (int)sizeof(header)) {
            ring_buffer_write(&conn->tx_buf, header, header_len);
            // 发送值
            ring_buffer_write(&conn->tx_buf, value, value_len);
            ring_buffer_write(&conn->tx_buf, "\r\n", 2);
            // 发送结束标记
            ring_buffer_write(&conn->tx_buf, "END\r\n", 5);
        } else {
            ring_buffer_write(&conn->tx_buf, "SERVER_ERROR response too large\r\n", 31);
        }
        free(value);
    } else {
        // 键不存在或已过期
        ring_buffer_write(&conn->tx_buf, "END\r\n", 5);
    }
}

static void handle_incr_command(memkv_conn_t* conn, const char* key, const char* value) {
    char* endptr;
    long increment = strtol(value, &endptr, 10);
    if (*endptr != '\0') {
        send_binary_error_response(conn, "INVALID_NUMBER");
        return;
    }

    void* stored_value;
    size_t value_len;
    if (poly_memkv_get(conn->store, key, &stored_value, &value_len) != POLY_OK) {
        send_binary_error_response(conn, "NOT_FOUND");
        return;
    }

    char* stored_str = (char*)stored_value;
    long current = strtol(stored_str, &endptr, 10);
    if (*endptr != '\0') {
        // 非数字值
        free(stored_value);
        send_binary_error_response(conn, "INVALID_NUMBER");
        return;
    }

    long result = current + increment;
    char new_value[32];
    int new_len = snprintf(new_value, sizeof(new_value), "%ld", result);

    if (poly_memkv_set(conn->store, key, new_value, new_len) != POLY_OK) {
        free(stored_value);
        send_binary_error_response(conn, "ERROR");
        return;
    }

    free(stored_value);
    char response[64];
    int response_len = snprintf(response, sizeof(response), "%ld\r\n", result);
    INFRA_LOG_DEBUG("INCR: Sending response: [%.*s]", response_len-2, response);
    ring_buffer_write(&conn->tx_buf, response, response_len);
}

static void send_binary_error_response(memkv_conn_t* conn, const char* error_msg) {
    char response[24];
    memset(response, 0, sizeof(response));

    // 设置响应头
    response[0] = 0x81;  // Magic: Response
    response[1] = 0x00;  // 操作码
    response[2] = 0x00;  // 状态: 无错误
    response[3] = 0x00;  // 状态: 无错误
    response[4] = 0x00;  // 键长度
    response[5] = 0x00;  // 额外长度
    response[6] = 0x00;  // 数据类型
    response[7] = 0x00;  // 保留
    response[8] = 0x00;  // 数据长度
    response[9] = 0x00;
    response[10] = 0x00;
    response[11] = 0x00;

    // 发送响应
    ring_buffer_write(&conn->tx_buf, response, sizeof(response));
}

static void handle_set_command(memkv_conn_t* conn, const char* key, const char* value, size_t value_len) {
    if (!conn || !key || !value || value_len == 0) {
        INFRA_LOG_ERROR("Invalid parameters for SET command");
        ring_buffer_write(&conn->tx_buf, "CLIENT_ERROR invalid parameters\r\n", 31);
        return;
    }

    INFRA_LOG_DEBUG("SET key=%s, value_len=%zu", key, value_len);
    
    infra_error_t err = set_with_expiry(conn->store, key, value, value_len, 0, 0);
    if (err == INFRA_OK) {
        ring_buffer_write(&conn->tx_buf, "STORED\r\n", 8);
    } else {
        INFRA_LOG_ERROR("SET failed: %d", err);
        ring_buffer_write(&conn->tx_buf, "NOT_STORED\r\n", 12);
    }
}

// 实现 incr/decr 功能
static infra_error_t poly_memkv_counter_op(poly_memkv_db_t* db, const char* key, int64_t delta, int64_t* new_value) {
    if (!db || !key || !new_value) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    void* value = NULL;
    size_t value_len = 0;
    infra_error_t err = get_with_expiry(db, key, &value, &value_len, NULL);
    
    if (err != INFRA_OK || !value) {
        return INFRA_ERROR_NOT_FOUND;
    }
    
    // 确保值以 null 结尾
    char* str_value = malloc(value_len + 1);
    if (!str_value) {
        free(value);
        return INFRA_ERROR_NO_MEMORY;
    }
    memcpy(str_value, value, value_len);
    str_value[value_len] = '\0';
    free(value);
    
    // 尝试将值转换为数字
    char* end;
    int64_t current = strtoll(str_value, &end, 10);
    if (*end != '\0') {
        // 非数字值
        free(str_value);
        return INFRA_ERROR_INVALID_PARAM;
    }
    free(str_value);
    
    // 应用增量
    *new_value = current + delta;
    if (*new_value < 0) *new_value = 0;  // memcached 协议规定不能为负
    
    // 将新值转换为字符串并存储
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%lld", *new_value);
    if (len < 0 || len >= sizeof(buf)) {
        return INFRA_ERROR_NO_MEMORY;
    }
    
    // 保持原有的过期时间和标志位
    err = set_with_expiry(db, key, buf, len, 0, 0);
    return err;
}

static infra_error_t poly_memkv_incr(poly_memkv_db_t* db, const char* key, int64_t delta, int64_t* new_value) {
    infra_error_t err = poly_memkv_counter_op(db, key, delta, new_value);
    if (err == INFRA_ERROR_NOT_FOUND) {
        // 如果键不存在，从增量值开始
        *new_value = delta;
        char buf[32];
        int len = snprintf(buf, sizeof(buf), "%lld", delta);
        if (len < 0 || len >= sizeof(buf)) {
            return INFRA_ERROR_NO_MEMORY;
        }
        err = set_with_expiry(db, key, buf, len, 0, 0);
        if (err != INFRA_OK) {
            return err;
        }
    }
    return err;
}

static infra_error_t poly_memkv_decr(poly_memkv_db_t* db, const char* key, int64_t delta, int64_t* new_value) {
    infra_error_t err = poly_memkv_counter_op(db, key, -delta, new_value);
    if (err == INFRA_ERROR_NOT_FOUND) {
        // 如果键不存在，从 0 开始
        *new_value = 0;
        err = set_with_expiry(db, key, "0", 1, 0, 0);
        if (err != INFRA_OK) {
            return err;
        }
    }
    return err;
}

// 检查键是否过期
static bool is_key_expired(poly_memkv_db_t* db, const char* key) {
    void* raw_value = NULL;
    size_t raw_len = 0;
    infra_error_t err = poly_memkv_get(db, key, &raw_value, &raw_len);
    
    if (err != INFRA_OK || !raw_value || raw_len < sizeof(memkv_item_t)) {
        if (raw_value) infra_free(raw_value);
        return true;
    }
    
    memkv_item_t* item = (memkv_item_t*)raw_value;
    bool expired = (item->expiry > 0 && item->expiry <= time(NULL));
    infra_free(raw_value);
    
    if (expired) {
        // 删除过期的键
        poly_memkv_del(db, key);
    }
    
    return expired;
}

// 设置键值对
static infra_error_t set_with_expiry(poly_memkv_db_t* db, const char* key, const void* value, 
                                    size_t value_len, uint32_t flags, time_t expiry) {
    if (!db || !key || !value || value_len == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 创建元数据结构
    memkv_item_t meta = {
        .expiry = expiry,
        .flags = flags,
        .value_len = value_len
    };

    // 分配存储空间
    size_t total_len = sizeof(memkv_item_t) + value_len;
    void* storage = malloc(total_len);
    if (!storage) {
        return INFRA_ERROR_NO_MEMORY;
    }

    // 复制元数据和值
    memcpy(storage, &meta, sizeof(memkv_item_t));
    memcpy((char*)storage + sizeof(memkv_item_t), value, value_len);

    // 存储到数据库
    infra_error_t err = poly_memkv_set(db, key, storage, total_len);
    free(storage);

    return err;
}

static infra_error_t get_with_expiry(poly_memkv_db_t* db, const char* key, void** value, 
                                    size_t* value_len, uint32_t* flags) {
    if (!db || !key || !value || !value_len) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    void* stored_data;
    size_t stored_len;
    infra_error_t err = poly_memkv_get(db, key, &stored_data, &stored_len);
    if (err != INFRA_OK) {
        return err;
    }

    // 检查存储的数据大小是否合理
    if (stored_len < sizeof(memkv_item_t)) {
        free(stored_data);
        return INFRA_ERROR_INVALID_STATE;  // 使用已定义的错误码
    }

    // 解析元数据
    memkv_item_t* meta = (memkv_item_t*)stored_data;
    
    // 检查是否过期
    if (meta->expiry > 0 && meta->expiry <= time(NULL)) {
        free(stored_data);
        poly_memkv_del(db, key);  // 删除过期的键
        return INFRA_ERROR_NOT_FOUND;
    }

    // 分配并复制值
    size_t data_len = stored_len - sizeof(memkv_item_t);
    void* data = malloc(data_len);
    if (!data) {
        free(stored_data);
        return INFRA_ERROR_NO_MEMORY;
    }

    memcpy(data, (char*)stored_data + sizeof(memkv_item_t), data_len);
    *value = data;
    *value_len = data_len;
    if (flags) {
        *flags = meta->flags;
    }

    free(stored_data);
    return INFRA_OK;
}

static void handle_flush_all_command(memkv_conn_t* conn) {
    if (!conn || !conn->store) {
        INFRA_LOG_ERROR("Invalid connection in handle_flush_all_command");
        return;
    }

    INFRA_LOG_DEBUG("Executing FLUSH_ALL command");
    
    // TODO: Implement actual flush_all functionality
    // For now, just return OK
    ring_buffer_write(&conn->tx_buf, "OK\r\n", 4);
}

static void handle_binary_command(memkv_conn_t* conn, const char* cmd, size_t cmd_len) {
    // 暂时不支持二进制协议
    INFRA_LOG_ERROR("Binary protocol not supported yet");
    send_binary_error_response(conn, "NOT_SUPPORTED");
}

static infra_error_t memkv_init(const infra_config_t* config) {
    INFRA_LOG_DEBUG("Initializing memkv service");
    if (!config) {
        INFRA_LOG_ERROR("Invalid config parameter");
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 初始化配置
    memset(&g_config, 0, sizeof(g_config));
    g_config.min_threads = MEMKV_MIN_THREADS;
    g_config.max_threads = MEMKV_MAX_THREADS;
    g_config.engine = "sqlite";  // 默认使用 sqlite 引擎
    g_config.plugin = NULL;

    // 如果有配置文件路径，加载配置
    const peer_service_config_t* service_config = (const peer_service_config_t*)config;
    if (service_config && service_config->config_path) {
        INFRA_LOG_INFO("Loading config from: %s", service_config->config_path);
        infra_error_t err = load_config(service_config->config_path);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to load config: %d", err);
            return err;
        }
    }

    // 更新服务状态
    g_memkv_service.state = SERVICE_STATE_STOPPED;
    g_initialized = true;
    INFRA_LOG_INFO("Memkv service initialized successfully");
    return INFRA_OK;
}

static infra_error_t memkv_cleanup(void) {
    if (!g_initialized) {
        return INFRA_OK;
    }

    // 停止服务
    if (g_config.running) {
        memkv_stop();
    }

    g_initialized = false;
    return INFRA_OK;
}

static infra_error_t memkv_start(void) {
    INFRA_LOG_DEBUG("Starting memkv service");
    if (!g_initialized) {
        INFRA_LOG_ERROR("Service not initialized");
        return INFRA_ERROR_NOT_INITIALIZED;
    }

    if (g_config.running) {
        INFRA_LOG_INFO("Service already running");
        return INFRA_OK;
    }

    // 更新服务状态
    g_memkv_service.state = SERVICE_STATE_STARTING;

    // 创建轮询上下文
    g_config.poll_ctx = (poly_poll_context_t*)malloc(sizeof(poly_poll_context_t));
    if (!g_config.poll_ctx) {
        INFRA_LOG_ERROR("Failed to allocate poll context");
        g_memkv_service.state = SERVICE_STATE_STOPPED;
        return INFRA_ERROR_NO_MEMORY;
    }

    // 初始化轮询上下文
    poly_poll_config_t poll_config = {
        .min_threads = g_config.min_threads,
        .max_threads = g_config.max_threads,
        .queue_size = 1024,
        .max_listeners = MEMKV_MAX_RULES
    };

    INFRA_LOG_DEBUG("Initializing poll context (threads: %d-%d, queue: %d)", 
                    poll_config.min_threads, poll_config.max_threads, poll_config.queue_size);

    infra_error_t err = poly_poll_init(g_config.poll_ctx, &poll_config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize poll context: %d", err);
        free(g_config.poll_ctx);
        g_config.poll_ctx = NULL;
        g_memkv_service.state = SERVICE_STATE_STOPPED;
        return err;
    }

    // 设置默认监听地址
    if (g_config.rule_count == 0) {
        INFRA_LOG_INFO("No rules configured, using default");
        strncpy(g_config.rules[0].addr, "127.0.0.1", MEMKV_MAX_ADDR_LEN - 1);
        g_config.rules[0].port = MEMKV_DEFAULT_PORT;
        g_config.rule_count = 1;
    }

    // 为每个规则添加监听器
    bool any_listener_added = false;
    for (int i = 0; i < g_config.rule_count; i++) {
        memkv_rule_t* rule = &g_config.rules[i];
        
        INFRA_LOG_DEBUG("Setting up listener %d: %s:%d", i, rule->addr, rule->port);
        
        // 创建监听器配置
        poly_poll_listener_t listener = {0};
        strncpy(listener.bind_addr, rule->addr, POLY_MAX_ADDR_LEN - 1);
        listener.bind_port = rule->port;
        listener.user_data = NULL;

        // 添加到轮询器
        if (poly_poll_add_listener(g_config.poll_ctx, &listener) != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to add listener for %s:%d", rule->addr, rule->port);
            continue;
        }

        INFRA_LOG_INFO("Successfully listening on %s:%d", rule->addr, rule->port);
        any_listener_added = true;
    }

    if (!any_listener_added) {
        INFRA_LOG_ERROR("Failed to add any listeners");
        poly_poll_cleanup(g_config.poll_ctx);
        free(g_config.poll_ctx);
        g_config.poll_ctx = NULL;
        g_memkv_service.state = SERVICE_STATE_STOPPED;
        return INFRA_ERROR_IO;
    }

    // 设置连接处理器
    INFRA_LOG_DEBUG("Setting up connection handler");
    poly_poll_set_handler(g_config.poll_ctx, handle_connection);

    // 启动轮询
    INFRA_LOG_DEBUG("Starting poll service");
    err = poly_poll_start(g_config.poll_ctx);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to start poll service: %d", err);
        poly_poll_cleanup(g_config.poll_ctx);
        free(g_config.poll_ctx);
        g_config.poll_ctx = NULL;
        g_memkv_service.state = SERVICE_STATE_STOPPED;
        return err;
    }

    g_config.running = true;
    g_memkv_service.state = SERVICE_STATE_RUNNING;
    INFRA_LOG_INFO("Memkv service started successfully");
    return INFRA_OK;
}

static infra_error_t memkv_stop(void) {
    if (!g_initialized || !g_config.running) {
        return INFRA_OK;
    }

    g_memkv_service.state = SERVICE_STATE_STOPPING;

    // 停止轮询
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

static infra_error_t load_config(const char* config_path) {
    INFRA_LOG_DEBUG("Loading config from: %s", config_path);
    if (!config_path) {
        INFRA_LOG_ERROR("Invalid config path");
        return INFRA_ERROR_INVALID_PARAM;
    }

    FILE* fp = fopen(config_path, "r");
    if (!fp) {
        INFRA_LOG_ERROR("Failed to open config file: %s (errno: %d)", config_path, errno);
        return INFRA_ERROR_IO;
    }

    char line[256];
    int line_no = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        line_no++;
        
        // 跳过空行和注释
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        
        INFRA_LOG_DEBUG("Processing config line %d: %s", line_no, line);
        
        // 解析配置行
        char addr[MEMKV_MAX_ADDR_LEN] = {0};
        int port = 0;
        char engine[32] = {0};
        char url[256] = {0};
        
        int matched = sscanf(line, "%s %d %s %s", addr, &port, engine, url);
        if (matched < 3) {
            INFRA_LOG_ERROR("Invalid config at line %d: %s", line_no, line);
            fclose(fp);
            return INFRA_ERROR_INVALID_PARAM;
        }
        
        INFRA_LOG_DEBUG("Parsed config: addr=%s, port=%d, engine=%s, url=%s", 
                       addr, port, engine, matched > 3 ? url : "");
        
        // 添加规则
        if (g_config.rule_count >= MEMKV_MAX_RULES) {
            INFRA_LOG_ERROR("Too many rules in config file");
            fclose(fp);
            return INFRA_ERROR_INVALID_PARAM;
        }
        
        memkv_rule_t* rule = &g_config.rules[g_config.rule_count++];
        strncpy(rule->addr, addr, MEMKV_MAX_ADDR_LEN - 1);
        rule->port = port;
        rule->binary_protocol = false;  // 默认使用文本协议
        
        // 设置存储引擎
        if (strcmp(engine, "sqlite") == 0) {
            g_config.engine = "sqlite";
        } else if (strcmp(engine, "duckdb") == 0) {
            g_config.engine = "duckdb";
        } else {
            INFRA_LOG_ERROR("Invalid engine type: %s", engine);
            fclose(fp);
            return INFRA_ERROR_INVALID_PARAM;
        }
        
        // 设置数据库 URL
        if (matched > 3) {
            g_config.plugin = strdup(url);
        }
        
        INFRA_LOG_INFO("Added rule: %s:%d (%s)", rule->addr, rule->port, g_config.engine);
    }
    
    fclose(fp);
    INFRA_LOG_INFO("Config loaded successfully with %d rules", g_config.rule_count);
    return INFRA_OK;
}

static infra_error_t memkv_cmd_handler(int argc, char* argv[]) {
    if (argc < 2) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 解析命令行参数
    const char* config_path = NULL;
    bool start = false;
    bool stop = false;
    bool status = false;

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

    // 执行命令
    if (start) {
        // 如果服务未初始化，先初始化
        if (!g_initialized) {
            peer_service_config_t init_config = {
                .name = "memkv",
                .type = SERVICE_TYPE_MEMKV,
                .options = memkv_options,
                .option_count = memkv_option_count,
                .config = NULL,
                .config_path = config_path
            };
            infra_error_t err = memkv_init((const infra_config_t*)&init_config);
            if (err != INFRA_OK) {
                return err;
            }
        } else if (config_path) {
            // 如果已经初始化但提供了新的配置文件，重新加载配置
            infra_error_t err = load_config(config_path);
            if (err != INFRA_OK) {
                return err;
            }
        }
        return memkv_start();
    } else if (stop) {
        return memkv_stop();
    } else if (status) {
        printf("memkv service is %s\n", memkv_is_running() ? "running" : "stopped");
        return INFRA_OK;
    }

    return INFRA_ERROR_INVALID_PARAM;
}

static void memkv_conn_destroy(memkv_conn_t* conn) {
    if (!conn) {
        return;
    }

    // 清理命令缓冲区
    if (conn->cmd_buf) {
        free(conn->cmd_buf);
        conn->cmd_buf = NULL;
    }

    // 清理环形缓冲区
    ring_buffer_cleanup(&conn->rx_buf);
    ring_buffer_cleanup(&conn->tx_buf);

    // 清理连接结构体
    free(conn);
}

memkv_conn_t* memkv_conn_create(infra_socket_t sock) {
    if (!sock) {
        INFRA_LOG_ERROR("Invalid socket in memkv_conn_create");
        return NULL;
    }

    memkv_conn_t* conn = (memkv_conn_t*)malloc(sizeof(memkv_conn_t));
    if (!conn) {
        INFRA_LOG_ERROR("Failed to allocate connection context");
        return NULL;
    }

    // 初始化连接结构
    memset(conn, 0, sizeof(memkv_conn_t));
    conn->sock = sock;
    conn->binary_protocol = false;
    conn->should_close = false;

    // 创建存储引擎实例
    poly_memkv_config_t config = {
        .engine = strcmp(g_config.engine, "duckdb") == 0 ? 
                 POLY_MEMKV_ENGINE_DUCKDB : POLY_MEMKV_ENGINE_SQLITE,
        .url = g_config.plugin ? g_config.plugin : ":memory:",
        .max_key_size = MEMKV_MAX_KEY_SIZE,
        .max_value_size = MEMKV_MAX_VALUE_SIZE,
        .memory_limit = 0,  // 无限制
        .enable_compression = false,
        .plugin_path = NULL,
        .allow_fallback = true,
        .read_only = false
    };

    infra_error_t err = poly_memkv_create(&config, &conn->store);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create memkv store");
        free(conn);
        return NULL;
    }

    // 初始化缓冲区
    ring_buffer_init(&conn->rx_buf);
    ring_buffer_init(&conn->tx_buf);

    if (!conn->rx_buf.data || !conn->tx_buf.data) {
        INFRA_LOG_ERROR("Failed to initialize ring buffers");
        if (conn->store) {
            poly_memkv_destroy(conn->store);
        }
        ring_buffer_cleanup(&conn->rx_buf);
        ring_buffer_cleanup(&conn->tx_buf);
        free(conn);
        return NULL;
    }

    return conn;
}
