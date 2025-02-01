#include "peer_service.h"
#include "../infra/infra_core.h"
#include "../infra/infra_net.h"
#include "../infra/infra_sync.h"
#include "../infra/infra_memory.h"
#include "../poly/poly_memkv.h"
#include "../poly/poly_poll.h"

// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <time.h>

#define MEMKV_VERSION "1.0.0"
#define MEMKV_MAX_ADDR_LEN 256
#define MEMKV_MAX_RULES 16
#define RING_BUFFER_SIZE 4096
#define MEMKV_BUFFER_SIZE 1048576  // 1MB
#define MEMKV_MAX_KEY_SIZE 250
#define MEMKV_MAX_VALUE_SIZE (1024 * 1024)  // 1MB
#define MEMKV_MIN_THREADS 32
#define MEMKV_MAX_THREADS 512
#define MEMKV_DEFAULT_PORT 11211
#define POLY_OK 0
#define POLY_ERR_NOT_FOUND 1

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
// Forward Declarations
//-----------------------------------------------------------------------------

static void handle_flush_all_command(memkv_conn_t* conn);
static void handle_get_command(memkv_conn_t* conn, const char* key);
static void handle_set_command(memkv_conn_t* conn, const char* key, const char* value, size_t value_len);
static void handle_text_command(memkv_conn_t* conn, const char* cmd);
static void handle_binary_command(memkv_conn_t* conn, const char* cmd, size_t cmd_len);
static void handle_connection(void* arg);
static infra_error_t memkv_init(const infra_config_t* config);
static infra_error_t memkv_cleanup(void);
static infra_error_t memkv_start(void);
static infra_error_t memkv_stop(void);
static bool memkv_is_running(void);
static infra_error_t memkv_cmd_handler(int argc, char* argv[]);
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
        .config = NULL
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
    len = len > free_space ? free_space : len;
    if (len == 0) return 0;
    
    // 计算第一段可写入长度
    size_t first_chunk = RING_BUFFER_SIZE - buf->write_pos;
    if (first_chunk >= len) {
        // 数据可以一次性写入
        memcpy(buf->data + buf->write_pos, data, len);
        buf->write_pos = (buf->write_pos + len) % RING_BUFFER_SIZE;
    } else {
        // 数据需要分两次写入
        memcpy(buf->data + buf->write_pos, data, first_chunk);
        memcpy(buf->data, data + first_chunk, len - first_chunk);
        buf->write_pos = len - first_chunk;
    }
    
    buf->bytes_available += len;
    return len;
}

static size_t ring_buffer_read(ring_buffer_t* buf, char* data, size_t len) {
    if (!buf || !buf->data || !data || len == 0) {
        return 0;
    }
    
    // 计算可读取的数据量
    len = len > buf->bytes_available ? buf->bytes_available : len;
    if (len == 0) return 0;
    
    // 计算第一段可读取长度
    size_t first_chunk = RING_BUFFER_SIZE - buf->read_pos;
    if (first_chunk >= len) {
        // 数据可以一次性读取
        memcpy(data, buf->data + buf->read_pos, len);
        buf->read_pos = (buf->read_pos + len) % RING_BUFFER_SIZE;
    } else {
        // 数据需要分两次读取
        memcpy(data, buf->data + buf->read_pos, first_chunk);
        memcpy(data + first_chunk, buf->data, len - first_chunk);
        buf->read_pos = len - first_chunk;
    }
    
    buf->bytes_available -= len;
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
    INFRA_LOG_DEBUG("New connection received, creating connection context");
    
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
            size_t sent;
            
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
                size_t sent2;
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
        size_t received;
        infra_error_t err = infra_net_recv(conn->sock, rx_buffer, RING_BUFFER_SIZE, &received);
        
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
            
            // 检查命令缓冲区是否有足够空间
            if (conn->cmd_len + received > MEMKV_BUFFER_SIZE) {
                INFRA_LOG_ERROR("Command too long");
                ring_buffer_write(&conn->tx_buf, "CLIENT_ERROR command too long\r\n", 30);
                conn->cmd_len = 0;
                continue;
            }
            
            // 追加到命令缓冲区
            memcpy(conn->cmd_buf + conn->cmd_len, rx_buffer, received);
            conn->cmd_len += received;
            
            // 查找命令结束符
            char* cmd_end;
            char* cmd_start = conn->cmd_buf;
            size_t remaining = conn->cmd_len;
            
            while ((cmd_end = memmem(cmd_start, remaining, "\r\n", 2)) != NULL) {
                size_t cmd_size = cmd_end - cmd_start + 2;  // 包含 \r\n
                handle_request(conn, cmd_start, cmd_size);
                
                cmd_start = cmd_end + 2;
                remaining = conn->cmd_len - (cmd_start - conn->cmd_buf);
            }
            
            // 移动未完成的命令到缓冲区开始
            if (cmd_start > conn->cmd_buf && remaining > 0) {
                memmove(conn->cmd_buf, cmd_start, remaining);
            }
            conn->cmd_len = remaining;
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
    INFRA_LOG_DEBUG("Received request: len=%zu, first_byte=0x%02x", len, (unsigned char)request[0]);
    
    // Print first few bytes for debugging
    char debug_buf[256] = {0};
    size_t debug_len = len > 32 ? 32 : len;
    for (size_t i = 0; i < debug_len; i++) {
        snprintf(debug_buf + i*3, sizeof(debug_buf) - i*3, "%02x ", (unsigned char)request[i]);
    }
    INFRA_LOG_DEBUG("Request hex dump: %s", debug_buf);

    // 检查缓冲区是否有足够空间
    if (RING_BUFFER_SIZE - conn->tx_buf.bytes_available < 1024) {
        INFRA_LOG_ERROR("Response buffer is nearly full");
        ring_buffer_write(&conn->tx_buf, "SERVER_ERROR buffer full\r\n", 25);
        return;
    }

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
        *cmd_end = '\0';  // 在 \r\n 处截断命令
        
        INFRA_LOG_DEBUG("Processing text protocol command: %s", cmd_buf);
        handle_text_command(conn, cmd_buf);
        free(cmd_buf);
    }
}

static void handle_text_command(memkv_conn_t* conn, const char* cmd) {
    if (!conn || !cmd || !conn->store) {
        INFRA_LOG_ERROR("Invalid text command parameters");
        return;
    }

    // Debug log text command
    INFRA_LOG_DEBUG("Processing text command: %.32s", cmd);

    // 跳过前导空格
    while (*cmd && isspace(*cmd)) cmd++;
    if (!*cmd) {
        INFRA_LOG_ERROR("Empty command");
        ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
        return;
    }

    // 分配临时缓冲区
    char* cmd_copy = strdup(cmd);
    if (!cmd_copy) {
        INFRA_LOG_ERROR("Failed to allocate memory for command");
        ring_buffer_write(&conn->tx_buf, "SERVER_ERROR out of memory\r\n", 27);
        return;
    }

    char* saveptr = NULL;
    char* token = strtok_r(cmd_copy, " \t", &saveptr);
    if (!token) {
        free(cmd_copy);
        ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
        return;
    }

    // 转换命令为大写
    char command[32] = {0};
    size_t cmd_len = strlen(token);
    if (cmd_len >= sizeof(command)) {
        free(cmd_copy);
        ring_buffer_write(&conn->tx_buf, "CLIENT_ERROR command too long\r\n", 30);
        return;
    }
    
    for (size_t i = 0; i < cmd_len; i++) {
        command[i] = toupper(token[i]);
    }

    INFRA_LOG_DEBUG("Parsed command: %s", command);

    if (strcmp(command, "GET") == 0) {
        char* key = strtok_r(NULL, " \t", &saveptr);
        if (key) {
            handle_get_command(conn, key);
        } else {
            ring_buffer_write(&conn->tx_buf, "CLIENT_ERROR no key\r\n", 20);
        }
    } else if (strcmp(command, "SET") == 0) {
        char* key = strtok_r(NULL, " \t", &saveptr);
        char* value = strtok_r(NULL, " \t", &saveptr);
        if (key && value) {
            if (set_with_expiry(conn->store, key, value, strlen(value), 0, 0) == INFRA_OK) {
                ring_buffer_write(&conn->tx_buf, "STORED\r\n", 8);
            } else {
                ring_buffer_write(&conn->tx_buf, "NOT_STORED\r\n", 12);
            }
        } else {
            ring_buffer_write(&conn->tx_buf, "CLIENT_ERROR missing key or value\r\n", 34);
        }
    } else if (strcmp(command, "DELETE") == 0) {
        char* key = strtok_r(NULL, " \t", &saveptr);
        if (key) {
            if (poly_memkv_del(conn->store, key) == INFRA_OK) {
                ring_buffer_write(&conn->tx_buf, "DELETED\r\n", 9);
            } else {
                ring_buffer_write(&conn->tx_buf, "NOT_FOUND\r\n", 11);
            }
        } else {
            ring_buffer_write(&conn->tx_buf, "CLIENT_ERROR no key\r\n", 20);
        }
    } else if (strcmp(command, "INCR") == 0) {
        char* key = strtok_r(NULL, " \t", &saveptr);
        char* value = strtok_r(NULL, " \t", &saveptr);
        if (key && value) {
            handle_incr_command(conn, key, value);
        } else {
            ring_buffer_write(&conn->tx_buf, "CLIENT_ERROR missing key or value\r\n", 34);
        }
    } else if (strcmp(command, "QUIT") == 0) {
        ring_buffer_write(&conn->tx_buf, "BYE\r\n", 5);
        conn->should_close = true;
    } else if (strcmp(command, "FLUSH_ALL") == 0) {
        handle_flush_all_command(conn);
    } else if (strncmp(command, "VALUE", 5) == 0) {
        // 忽略 VALUE 响应，这是 GET 命令的一部分
        INFRA_LOG_DEBUG("Ignoring VALUE response part");
    } else {
        INFRA_LOG_ERROR("Unknown text command: %s", command);
        ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
    }

    free(cmd_copy);
}

static void handle_get_command(memkv_conn_t* conn, const char* key) {
    if (!conn || !key || !conn->store) {
        INFRA_LOG_ERROR("Invalid parameters in handle_get_command");
        return;
    }

    INFRA_LOG_DEBUG("GET: key=[%s]", key);

    void* value = NULL;
    size_t value_len = 0;
    uint32_t flags = 0;

    infra_error_t err = get_with_expiry(conn->store, key, &value, &value_len, &flags);
    
    if (err == INFRA_OK && value) {
        // 构造响应
        char header[256];
        int header_len = snprintf(header, sizeof(header), "VALUE %s %u %zu\r\n", 
                                key, flags, value_len);
        
        // 发送响应头
        ring_buffer_write(&conn->tx_buf, header, header_len);
        
        // 发送值
        ring_buffer_write(&conn->tx_buf, value, value_len);
        ring_buffer_write(&conn->tx_buf, "\r\n", 2);
        
        // 发送结束标记
        ring_buffer_write(&conn->tx_buf, "END\r\n", 5);
        
        free(value);
    } else {
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
    if (!db || !key || (!value && value_len > 0)) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 计算总长度并分配内存
    size_t total_len = sizeof(memkv_item_t) + value_len;
    char* buffer = malloc(total_len);
    if (!buffer) {
        return INFRA_ERROR_NO_MEMORY;
    }
    
    // 设置元数据
    memkv_item_t* item = (memkv_item_t*)buffer;
    item->expiry = expiry > 0 ? time(NULL) + expiry : 0;
    item->flags = flags;
    item->value_len = value_len;
    
    // 复制值数据
    if (value && value_len > 0) {
        memcpy(buffer + sizeof(memkv_item_t), value, value_len);
    }
    
    // 存储到数据库
    infra_error_t err = poly_memkv_set(db, key, buffer, total_len);
    free(buffer);
    return err;
}

// 获取键值对
static infra_error_t get_with_expiry(poly_memkv_db_t* db, const char* key, void** value, 
                                    size_t* value_len, uint32_t* flags) {
    if (!db || !key || !value || !value_len) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    *value = NULL;
    *value_len = 0;
    if (flags) *flags = 0;

    void* raw_value = NULL;
    size_t raw_len = 0;
    infra_error_t err = poly_memkv_get(db, key, &raw_value, &raw_len);
    
    if (err != INFRA_OK || !raw_value || raw_len < sizeof(memkv_item_t)) {
        if (raw_value) infra_free(raw_value);
        return INFRA_ERROR_NOT_FOUND;
    }
    
    memkv_item_t* item = (memkv_item_t*)raw_value;
    
    // 检查是否过期
    if (item->expiry > 0 && item->expiry <= time(NULL)) {
        infra_free(raw_value);
        poly_memkv_del(db, key);  // 删除过期的键
        return INFRA_ERROR_NOT_FOUND;
    }
    
    if (flags) *flags = item->flags;
    
    // 分配内存并复制实际的值
    *value_len = item->value_len;
    *value = malloc(*value_len);
    if (!*value) {
        infra_free(raw_value);
        return INFRA_ERROR_NO_MEMORY;
    }
    
    memcpy(*value, (char*)raw_value + sizeof(memkv_item_t), *value_len);
    infra_free(raw_value);
    return INFRA_OK;
}

// 发送响应
static bool send_response(infra_socket_t sock, const char* response, size_t len) {
    size_t total_sent = 0;
    while (total_sent < len) {
        ssize_t sent = send(infra_net_get_fd(sock), response + total_sent, len - total_sent, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;  // 被信号中断,重试
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;  // 非阻塞模式下暂时无法写入,重试
            }
            if (errno == EPIPE || errno == ECONNRESET) {
                INFRA_LOG_DEBUG("Connection closed by peer");
                return false;  // 连接已关闭,直接返回
            }
            INFRA_LOG_ERROR("Failed to send response: %d", errno);
            return false;
        }
        total_sent += sent;
    }
    return true;
}

// 处理客户端请求
static void handle_client_command(memkv_conn_t* conn, const char* cmd) {
    if (!conn || !cmd) {
        INFRA_LOG_ERROR("Invalid client command parameters");
        return;
    }

    // Debug log client command
    INFRA_LOG_DEBUG("Client command: [%s]", cmd);

    // ... handle client command ...
}

static void handle_binary_command(memkv_conn_t* conn, const char* cmd, size_t cmd_len) {
    if (!conn || !cmd || cmd_len == 0) {
        INFRA_LOG_ERROR("Invalid arguments to handle_binary_command");
        return;
    }

    // For now, just send an error response since binary protocol is not fully implemented
    send_binary_error_response(conn, "Binary protocol not implemented yet");
}

//-----------------------------------------------------------------------------
// Service Implementation
//-----------------------------------------------------------------------------

// 启动服务
static infra_error_t memkv_start(void) {
    if (g_config.running) {
        return INFRA_OK;
    }

    infra_error_t err = INFRA_OK;

    // 初始化监听器
    for (int i = 0; i < g_config.rule_count; i++) {
        memkv_rule_t* rule = &g_config.rules[i];

        // 创建监听器配置
        poly_poll_listener_t poll_listener = {0};
        strncpy(poll_listener.bind_addr, rule->addr, POLY_MAX_ADDR_LEN - 1);
        poll_listener.bind_port = rule->port;
        poll_listener.user_data = rule;

        // 添加到poll上下文
        err = poly_poll_add_listener(g_config.poll_ctx, &poll_listener);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to add listener: %d", err);
            goto cleanup;
        }

        INFRA_LOG_INFO("Listening on %s:%d", rule->addr, rule->port);
    }

    // 设置连接处理回调
    poly_poll_set_handler(g_config.poll_ctx, handle_connection);

    // 启动poll
    err = poly_poll_start(g_config.poll_ctx);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to start poll: %d", err);
        goto cleanup;
    }

    g_config.running = true;
    return INFRA_OK;

cleanup:
    return err;
}

// 停止服务
static infra_error_t memkv_stop(void) {
    if (!g_config.running) {
        return INFRA_OK;
    }

    // Stop poly_poll first
    infra_error_t err = poly_poll_stop(g_config.poll_ctx);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to stop poll: %d", err);
        return err;
    }

    g_config.running = false;
    return INFRA_OK;
}

// 检查服务是否运行
bool memkv_is_running(void) {
    return g_config.running;
}

// 处理命令行
infra_error_t memkv_cmd_handler(int argc, char* argv[]) {
    if (argc < 1) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 初始化服务
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    infra_error_t err = memkv_init(&config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize memkv service: %d", err);
        return err;
    }

    // 解析命令行参数
    bool should_start = false;
    const char* config_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--start") == 0) {
            should_start = true;
        } else if (strcmp(argv[i], "--stop") == 0) {
            err = memkv_stop();
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to stop memkv service: %d", err);
                return err;
            }
            err = memkv_cleanup();
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to cleanup memkv service: %d", err);
                return err;
            }
            INFRA_LOG_INFO("MemKV service stopped successfully");
            return INFRA_OK;
        } else if (strcmp(argv[i], "--status") == 0) {
            if (g_config.running) {
                INFRA_LOG_INFO("Service is running with %d rules:", g_config.rule_count);
                for (int j = 0; j < g_config.rule_count; j++) {
                    memkv_rule_t* rule = &g_config.rules[j];
                    INFRA_LOG_INFO("  Rule %d: %s:%d", j,
                        rule->addr, rule->port);
                }
            } else {
                INFRA_LOG_INFO("Service is stopped");
            }
            return INFRA_OK;
        } else if (strncmp(argv[i], "--config=", 9) == 0) {
            config_path = argv[i] + 9;
        }
    }

    // 如果需要启动服务
    if (should_start) {
        // 加载配置
        if (config_path) {
            err = load_config(config_path);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to load config: %d, using default configuration", err);
            }
        } else {
            INFRA_LOG_INFO("No config file specified, using default configuration");
        }

        // 启动服务
        err = memkv_start();
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to start memkv service: %d", err);
            return err;
        }

        INFRA_LOG_INFO("MemKV service started successfully");
    }

    return INFRA_OK;
}

infra_error_t peer_memkv_open(poly_memkv_db_t** db, const char* path) {
    if (!db || !path) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    poly_memkv_db_t* new_db = infra_malloc(sizeof(poly_memkv_db_t));
    if (!new_db) {
        return INFRA_ERROR_NO_MEMORY;
    }
    
    // 初始化数据库
    poly_memkv_config_t config = {
        .url = path,
        .engine = POLY_MEMKV_ENGINE_SQLITE,
        .max_key_size = MEMKV_MAX_KEY_SIZE,
        .max_value_size = MEMKV_MAX_VALUE_SIZE,
        .memory_limit = 0,  // 无限制
        .enable_compression = false,
        .plugin_path = NULL,
        .allow_fallback = true,
        .read_only = false
    };
    
    infra_error_t err = poly_memkv_create(&config, db);
    if (err != INFRA_OK) {
        infra_free(new_db);
        return err;
    }
    
    return INFRA_OK;
}

void peer_memkv_close(poly_memkv_db_t* db) {
    if (db) {
        poly_memkv_destroy(db);
    }
}

infra_error_t peer_memkv_get(poly_memkv_db_t* db, const char* key, void** value, size_t* value_len) {
    if (!db || !key || !value || !value_len) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    uint32_t flags;
    return get_with_expiry(db, key, value, value_len, &flags);
}

infra_error_t peer_memkv_set(poly_memkv_db_t* db, const char* key, const void* value, size_t value_len) {
    if (!db || !key || (!value && value_len > 0)) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    return set_with_expiry(db, key, value, value_len, 0, 0);
}

infra_error_t peer_memkv_del(poly_memkv_db_t* db, const char* key) {
    if (!db || !key) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    // 检查是否过期
    void* value = NULL;
    size_t value_len = 0;
    infra_error_t err = get_with_expiry(db, key, &value, &value_len, NULL);
    
    if (err == INFRA_ERROR_NOT_FOUND) {
        return err;
    }
    
    if (value) {
        free(value);
    }
    
    return poly_memkv_del(db, key);
}

infra_error_t peer_memkv_iter_create(poly_memkv_db_t* db, poly_memkv_iter_t** iter) {
    if (!db || !iter) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    poly_memkv_iter_t* new_iter = infra_malloc(sizeof(poly_memkv_iter_t));
    if (!new_iter) {
        return INFRA_ERROR_NO_MEMORY;
    }
    
    infra_error_t err = poly_memkv_iter_create(db, iter);
    if (err != INFRA_OK) {
        infra_free(new_iter);
        return err;
    }
    
    return INFRA_OK;
}

infra_error_t peer_memkv_iter_next(poly_memkv_iter_t* iter, char** key, void** value, size_t* value_len) {
    if (!iter || !key || !value || !value_len) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    *key = NULL;
    *value = NULL;
    *value_len = 0;
    
    // 获取下一个键值对
    infra_error_t err = poly_memkv_iter_next(iter, key, value, value_len);
    if (err != INFRA_OK) {
        return err;
    }
    
    // 如果值包含元数据，需要提取实际的值
    if (*value && *value_len >= sizeof(memkv_item_t)) {
        memkv_item_t* item = (memkv_item_t*)*value;
        
        // 检查是否过期
        if (item->expiry > 0 && item->expiry <= time(NULL)) {
            infra_free(*value);
            *value = NULL;
            *value_len = 0;
            return INFRA_ERROR_NOT_FOUND;
        }
        
        // 提取实际的值
        void* real_value = malloc(item->value_len);
        if (!real_value) {
            infra_free(*value);
            *value = NULL;
            *value_len = 0;
            return INFRA_ERROR_NO_MEMORY;
        }
        
        memcpy(real_value, (char*)*value + sizeof(memkv_item_t), item->value_len);
        infra_free(*value);
        *value = real_value;
        *value_len = item->value_len;
    }
    
    return INFRA_OK;
}

void peer_memkv_iter_destroy(poly_memkv_iter_t* iter) {
    if (iter) {
        poly_memkv_iter_destroy(iter);
    }
}

// 初始化服务
static infra_error_t memkv_init(const infra_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 初始化上下文
    memset(&g_config, 0, sizeof(g_config));

    // Create and initialize poll context
    g_config.poll_ctx = (poly_poll_context_t*)malloc(sizeof(poly_poll_context_t));
    if (!g_config.poll_ctx) {
        INFRA_LOG_ERROR("Failed to allocate poll context");
        return INFRA_ERROR_NO_MEMORY;
    }
    memset(g_config.poll_ctx, 0, sizeof(poly_poll_context_t));

    // Initialize poly_poll
    poly_poll_config_t poll_config = {
        .min_threads = MEMKV_MIN_THREADS,
        .max_threads = MEMKV_MAX_THREADS,
        .queue_size = 1024,
        .max_listeners = MEMKV_MAX_RULES
    };

    infra_error_t err = poly_poll_init(g_config.poll_ctx, &poll_config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to init poll: %d", err);
        free(g_config.poll_ctx);
        g_config.poll_ctx = NULL;
        return err;
    }

    // 使用默认配置
    memkv_rule_t* rule = &g_config.rules[0];
    memset(rule, 0, sizeof(memkv_rule_t));
    strncpy(rule->addr, "127.0.0.1", sizeof(rule->addr) - 1);
    rule->port = MEMKV_DEFAULT_PORT;
    rule->binary_protocol = false;
    g_config.rule_count = 1;

    return INFRA_OK;
}

// 加载配置
static infra_error_t load_config(const char* config_path) {
    FILE* fp = fopen(config_path, "r");
    if (!fp) {
        INFRA_LOG_ERROR("Failed to open config file: %s", config_path);
        return INFRA_ERROR_IO;
    }

    char line[1024];
    int line_num = 0;
    infra_error_t err = INFRA_OK;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        // 解析配置行...
    }

    fclose(fp);
    return err;
}

// 清理服务
static infra_error_t memkv_cleanup(void) {
    // 停止服务
    if (g_config.running) {
        memkv_stop();
    }

    // Cleanup poly_poll
    if (g_config.poll_ctx) {
        poly_poll_cleanup(g_config.poll_ctx);
        free(g_config.poll_ctx);
        g_config.poll_ctx = NULL;
    }

    return INFRA_OK;
}

memkv_conn_t* memkv_conn_create(infra_socket_t sock) {
    memkv_conn_t* conn = (memkv_conn_t*)malloc(sizeof(memkv_conn_t));
    if (!conn) {
        return NULL;
    }

    memset(conn, 0, sizeof(memkv_conn_t));
    conn->sock = sock;
    conn->binary_protocol = false;
    conn->should_close = false;

    // Initialize ring buffers
    ring_buffer_init(&conn->rx_buf);
    ring_buffer_init(&conn->tx_buf);
    
    // Check if ring buffers were initialized successfully
    if (!conn->rx_buf.data || !conn->tx_buf.data) {
        INFRA_LOG_ERROR("Failed to initialize ring buffers");
        ring_buffer_cleanup(&conn->rx_buf);
        ring_buffer_cleanup(&conn->tx_buf);
        free(conn);
        return NULL;
    }

    // Initialize command buffer
    conn->cmd_buf = (char*)malloc(MEMKV_BUFFER_SIZE);
    if (!conn->cmd_buf) {
        INFRA_LOG_ERROR("Failed to allocate command buffer");
        ring_buffer_cleanup(&conn->rx_buf);
        ring_buffer_cleanup(&conn->tx_buf);
        free(conn);
        return NULL;
    }
    conn->cmd_len = 0;

    // Initialize store
    poly_memkv_config_t store_config = {
        .url = ":memory:",  // 使用内存数据库
        .engine = POLY_MEMKV_ENGINE_SQLITE,
        .max_key_size = MEMKV_MAX_KEY_SIZE,
        .max_value_size = MEMKV_MAX_VALUE_SIZE,
        .memory_limit = 0,  // 无限制
        .enable_compression = false,
        .plugin_path = NULL,
        .allow_fallback = true,
        .read_only = false
    };

    infra_error_t err = poly_memkv_create(&store_config, &conn->store);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create memkv store: %d", err);
        free(conn->cmd_buf);
        ring_buffer_cleanup(&conn->rx_buf);
        ring_buffer_cleanup(&conn->tx_buf);
        free(conn);
        return NULL;
    }

    return conn;
}

static void memkv_conn_destroy(memkv_conn_t* conn) {
    if (conn) {
        if (conn->store) {
            poly_memkv_destroy(conn->store);
            conn->store = NULL;
        }
        if (conn->cmd_buf) {
            free(conn->cmd_buf);
            conn->cmd_buf = NULL;
        }
        ring_buffer_cleanup(&conn->rx_buf);
        ring_buffer_cleanup(&conn->tx_buf);
        free(conn);
    }
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
