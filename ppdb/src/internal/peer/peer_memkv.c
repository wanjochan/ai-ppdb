#include "internal/peer/peer_service.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_memory.h"
#include "internal/poly/poly_memkv.h"

//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------

static bool send_response(infra_socket_t sock, const char* response, size_t len);

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define MEMKV_VERSION          "1.0.0"
#define MEMKV_BUFFER_SIZE      1048576  // 增加到 1MB
#define MEMKV_MAX_KEY_SIZE     250
#define MEMKV_MAX_VALUE_SIZE   (1024 * 1024)  // 1MB
#define MEMKV_MIN_THREADS      32
#define MEMKV_MAX_THREADS      512
#define MEMKV_DEFAULT_PORT     11211
#define MEMKV_MAX_ADDR_LEN      256
#define MEMKV_MAX_RULES        16

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// 存储规则
typedef struct memkv_rule {
    char bind_addr[MEMKV_MAX_ADDR_LEN];  // 绑定地址
    uint16_t bind_port;                  // 绑定端口
    poly_memkv_engine_t engine;          // 存储引擎
    char* db_path;                       // 数据库路径
    char* plugin_path;                   // 插件路径
    size_t max_memory;                   // 内存限制
    bool enable_compression;             // 启用压缩
    bool read_only;                      // 只读模式
} memkv_rule_t;

// 键值对元数据
typedef struct memkv_item {
    time_t expiry;                       // 过期时间
    uint32_t flags;                      // 标志位
    size_t value_len;                    // 值长度
} memkv_item_t;

// 连接上下文
typedef struct memkv_conn {
    infra_socket_t client;              // 客户端socket
    memkv_rule_t* rule;                 // 关联的规则
    poly_memkv_db_t* store;             // 存储实例
    char buffer[MEMKV_BUFFER_SIZE];     // 数据缓冲区
    size_t buffer_used;                 // 缓冲区已使用大小
    char* cmd_start;                    // 当前命令开始位置
    bool in_command;                    // 是否正在处理命令
} memkv_conn_t;

// 服务上下文
typedef struct memkv_context {
    bool running;                        // 运行标志
    infra_thread_pool_t* pool;          // 线程池
    memkv_rule_t rules[MEMKV_MAX_RULES];// 规则列表
    int rule_count;                      // 规则数量
    infra_socket_t listeners[MEMKV_MAX_RULES]; // 监听socket列表
    struct pollfd polls[MEMKV_MAX_RULES];      // poll事件数组
} memkv_context_t;

// Peer Memory KV Store handle implementation
typedef struct peer_memkv_db {
    poly_memkv_db_t* db;  // Underlying poly_memkv handle
} peer_memkv_db_t;

// Peer Memory KV Store iterator implementation
typedef struct peer_memkv_iter {
    poly_memkv_iter_t* iter;  // Underlying poly_memkv iterator
} peer_memkv_iter_t;

//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------

// Service interface functions
static infra_error_t memkv_init(const infra_config_t* config);
static infra_error_t memkv_cleanup(void);
static infra_error_t memkv_start(void);
static infra_error_t memkv_stop(void);
static bool memkv_is_running(void);
static infra_error_t memkv_cmd_handler(int argc, char* argv[]);

// Helper functions
static infra_error_t load_config(const char* config_path);
static void* handle_connection(void* arg);

// Storage functions
static bool is_key_expired(poly_memkv_db_t* db, const char* key);
static infra_error_t set_with_expiry(poly_memkv_db_t* db, const char* key, const void* value, 
                                    size_t value_len, uint32_t flags, time_t expiry);
static infra_error_t get_with_expiry(poly_memkv_db_t* db, const char* key, void** value, 
                                    size_t* value_len, uint32_t* flags);
static infra_error_t poly_memkv_counter_op(poly_memkv_db_t* db, const char* key, int64_t delta, int64_t* new_value);
static infra_error_t poly_memkv_incr(poly_memkv_db_t* db, const char* key, int64_t delta, int64_t* new_value);
static infra_error_t poly_memkv_decr(poly_memkv_db_t* db, const char* key, int64_t delta, int64_t* new_value);

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

// 服务上下文
static memkv_context_t g_context = {0};  // 静态初始化为 0

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

// 处理客户端连接
static void* handle_connection(void* arg) {
    memkv_conn_t* conn = (memkv_conn_t*)arg;
    if (!conn) {
        return NULL;
    }

    INFRA_LOG_DEBUG("Started connection handling on port %d", 
        conn->rule->bind_port);

    // 设置客户端socket为阻塞模式
    infra_net_set_nonblock(conn->client, false);
    infra_net_set_timeout(conn->client, 30000);  // 30秒超时

    // 创建存储实例
    poly_memkv_config_t config = {
        .engine = conn->rule->engine,
        .url = conn->rule->db_path,
        .max_key_size = MEMKV_MAX_KEY_SIZE,
        .max_value_size = MEMKV_MAX_VALUE_SIZE,
        .memory_limit = conn->rule->max_memory,
        .enable_compression = conn->rule->enable_compression,
        .plugin_path = conn->rule->plugin_path,
        .allow_fallback = true,
        .read_only = conn->rule->read_only
    };

    infra_error_t err = poly_memkv_create(&config, &conn->store);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create storage instance: %d", err);
        const char* error = "SERVER_ERROR failed to create storage\r\n";
        send_response(conn->client, error, strlen(error));
        goto cleanup;
    }

    conn->buffer_used = 0;
    conn->cmd_start = conn->buffer;
    
    while (g_context.running) {
        // 读取命令
        ssize_t bytes_read = recv(infra_net_get_fd(conn->client), 
                                conn->buffer + conn->buffer_used,
                                sizeof(conn->buffer) - conn->buffer_used - 1, 0);
                                
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                // 客户端正常关闭连接
                INFRA_LOG_DEBUG("Client closed connection");
                break;
            }
            if (errno == EINTR) {
                // 被信号中断，继续读取
                continue;
            }
            if (errno == ECONNRESET) {
                // 连接被重置,正常退出
                INFRA_LOG_DEBUG("Connection reset by peer");
                break;
            }
            // 其他错误，记录并退出
            INFRA_LOG_ERROR("Read error: %d", errno);
            break;
        }

        conn->buffer_used += bytes_read;
        conn->buffer[conn->buffer_used] = '\0';
        
        // 处理所有完整的命令
        while (conn->cmd_start < conn->buffer + conn->buffer_used) {
            // 寻找命令结束标记
            char* cmd_end = strstr(conn->cmd_start, "\r\n");
            if (!cmd_end) {
                // 命令不完整，移动到缓冲区开始
                if (conn->cmd_start != conn->buffer) {
                    conn->buffer_used = conn->buffer + conn->buffer_used - conn->cmd_start;
                    memmove(conn->buffer, conn->cmd_start, conn->buffer_used);
                    conn->cmd_start = conn->buffer;
                }
                break;
            }
            *cmd_end = '\0';
            
            // 解析命令
            char cmd[32] = {0};
            char key[MEMKV_MAX_KEY_SIZE] = {0};
            char flags_str[32] = {0};
            char exptime_str[32] = {0};
            char bytes_str[32] = {0};
            size_t value_len = 0;
            
            if (sscanf(conn->cmd_start, "set %s %s %s %s", key, flags_str, exptime_str, bytes_str) == 4) {
                // SET command
                value_len = atoi(bytes_str);
                if (value_len > MEMKV_MAX_VALUE_SIZE) {
                    if (!send_response(conn->client, "CLIENT_ERROR value too large\r\n", 28)) {
                        goto cleanup;
                    }
                    conn->cmd_start = cmd_end + 2;
                    continue;
                }
                
                // Check if we have enough data
                char* value_start = cmd_end + 2;
                if (value_start + value_len + 2 > conn->buffer + conn->buffer_used) {
                    // Incomplete data, restore \r\n and wait for more
                    *cmd_end = '\r';
                    if (conn->cmd_start != conn->buffer) {
                        conn->buffer_used = conn->buffer + conn->buffer_used - conn->cmd_start;
                        memmove(conn->buffer, conn->cmd_start, conn->buffer_used);
                        conn->cmd_start = conn->buffer;
                    }
                    break;
                }
                
                // Verify data chunk ends with \r\n
                if (value_start[value_len] != '\r' || value_start[value_len + 1] != '\n') {
                    if (!send_response(conn->client, "CLIENT_ERROR bad data chunk\r\n", 28)) {
                        goto cleanup;
                    }
                    conn->cmd_start = value_start + value_len + 2;
                    continue;
                }

                // Create temporary buffer for value (without trailing \r\n)
                char* value_buf = malloc(value_len + 1);  // +1 for null terminator
                if (!value_buf) {
                    if (!send_response(conn->client, "SERVER_ERROR out of memory\r\n", 26)) {
                        goto cleanup;
                    }
                    conn->cmd_start = value_start + value_len + 2;
                    continue;
                }
                memcpy(value_buf, value_start, value_len);
                value_buf[value_len] = '\0';  // Null terminate for numeric operations

                // Store value with expiry
                uint32_t flags = atoi(flags_str);
                time_t expiry = atoi(exptime_str);
                if (expiry > 0) {
                    if (expiry <= 2592000) {  // 30 days in seconds
                        expiry = time(NULL) + expiry;
                    }
                }
                err = set_with_expiry(conn->store, key, value_buf, value_len, flags, expiry);
                free(value_buf);

                if (!send_response(conn->client, err == INFRA_OK ? "STORED\r\n" : "NOT_STORED\r\n", 
                            err == INFRA_OK ? 8 : 12)) {
                    goto cleanup;
                }
                conn->cmd_start = value_start + value_len + 2;
                continue;
                
            } else if (sscanf(conn->cmd_start, "get %s", key) == 1 || sscanf(conn->cmd_start, "gets %s", key) == 1) {
                // GET/GETS command
                void* value = NULL;
                size_t value_len = 0;
                uint32_t flags = 0;
                err = get_with_expiry(conn->store, key, &value, &value_len, &flags);
                
                if (err == INFRA_OK && value != NULL) {
                    // Send response header
                    char header[64];
                    int header_len = snprintf(header, sizeof(header), 
                                     "VALUE %s %u %zu\r\n", key, flags, value_len);
                    if (!send_response(conn->client, header, header_len) ||
                        !send_response(conn->client, value, value_len) ||
                        !send_response(conn->client, "\r\n", 2)) {
                        free(value);
                        goto cleanup;
                    }
                    free(value);
                }
                
                // Send END marker
                if (!send_response(conn->client, "END\r\n", 5)) {
                    goto cleanup;
                }
                conn->cmd_start = cmd_end + 2;
                continue;
                
            } else if (sscanf(conn->cmd_start, "delete %s", key) == 1) {
                // DELETE command
                err = poly_memkv_del(conn->store, key);
                if (!send_response(conn->client, err == INFRA_OK ? "DELETED\r\n" : "NOT_FOUND\r\n",
                            err == INFRA_OK ? 9 : 11)) {
                    goto cleanup;
                }
                conn->cmd_start = cmd_end + 2;
                continue;
                
            } else if (strncmp(conn->cmd_start, "flush_all", 9) == 0) {
                // FLUSH_ALL command - 清空所有数据
                if (conn->store) {
                    poly_memkv_destroy(conn->store);
                    err = poly_memkv_create(&config, &conn->store);
                }
                if (!send_response(conn->client, "OK\r\n", 4)) {
                    goto cleanup;
                }
                conn->cmd_start = cmd_end + 2;
                continue;
                
            } else if (sscanf(conn->cmd_start, "incr %s %s", key, bytes_str) == 2) {
                // INCREMENT command
                int64_t delta = atoll(bytes_str);
                int64_t new_value;
                err = poly_memkv_incr(conn->store, key, delta, &new_value);
                if (err == INFRA_OK) {
                    char response[32];
                    int len = snprintf(response, sizeof(response), "%lld\r\n", new_value);
                    if (!send_response(conn->client, response, len)) {
                        goto cleanup;
                    }
                } else {
                    if (!send_response(conn->client, "NOT_FOUND\r\n", 11)) {
                        goto cleanup;
                    }
                }
                conn->cmd_start = cmd_end + 2;
                continue;
                
            } else if (sscanf(conn->cmd_start, "decr %s %s", key, bytes_str) == 2) {
                // DECREMENT command
                int64_t delta = atoll(bytes_str);
                int64_t new_value;
                err = poly_memkv_decr(conn->store, key, delta, &new_value);
                if (err == INFRA_OK) {
                    char response[32];
                    int len = snprintf(response, sizeof(response), "%lld\r\n", new_value);
                    if (!send_response(conn->client, response, len)) {
                        goto cleanup;
                    }
                } else {
                    if (!send_response(conn->client, "NOT_FOUND\r\n", 11)) {
                        goto cleanup;
                    }
                }
                conn->cmd_start = cmd_end + 2;
                continue;
                
            } else {
                // Unknown command
                if (!send_response(conn->client, "ERROR\r\n", 7)) {
                    goto cleanup;
                }
                conn->cmd_start = cmd_end + 2;
                continue;
            }
        }
        
        // 移动未完成的命令到缓冲区开始
        if (conn->cmd_start < conn->buffer + conn->buffer_used) {
            conn->buffer_used = conn->buffer + conn->buffer_used - conn->cmd_start;
            memmove(conn->buffer, conn->cmd_start, conn->buffer_used);
            conn->cmd_start = conn->buffer;
        } else {
            conn->buffer_used = 0;
            conn->cmd_start = conn->buffer;
        }
    }

cleanup:
    // 清理连接
    if (conn->store) {
        poly_memkv_destroy(conn->store);
        conn->store = NULL;
    }
    if (conn->client) {
        infra_net_close(conn->client);
        conn->client = NULL;
    }
    free(conn);
    return NULL;
}

//-----------------------------------------------------------------------------
// Service Implementation
//-----------------------------------------------------------------------------

// 初始化服务
static infra_error_t memkv_init(const infra_config_t* config) {
    if (!config) return INFRA_ERROR_INVALID_PARAM;

    // 初始化上下文
    memset(&g_context, 0, sizeof(g_context));
    g_context.running = false;
    g_context.rule_count = 0;

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
        
        // 跳过空行和注释
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        // 移除行尾的换行符
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        // 解析规则
        char bind_addr[MEMKV_MAX_ADDR_LEN] = {0};
        int bind_port = 0;
        char engine_str[32] = {0};
        char db_path[1024] = {0};
        char plugin_path[1024] = {0};
        size_t max_memory = 0;
        int enable_compression = 0;
        int read_only = 0;

        // 尝试解析完整格式
        int matched = sscanf(line, "%s %d %s %s %s %zu %d %d", 
            bind_addr, &bind_port, engine_str, db_path, plugin_path, 
            &max_memory, &enable_compression, &read_only);

        if (matched < 3) {
            INFRA_LOG_ERROR("Invalid config at line %d: %s", line_num, line);
            err = INFRA_ERROR_INVALID_PARAM;
            break;
        }

        // 检查是否达到最大规则数
        if (g_context.rule_count >= MEMKV_MAX_RULES) {
            INFRA_LOG_ERROR("Too many rules (max %d)", MEMKV_MAX_RULES);
            err = INFRA_ERROR_NO_MEMORY;
            break;
        }

        // 创建新规则
        memkv_rule_t* rule = &g_context.rules[g_context.rule_count];
        memset(rule, 0, sizeof(memkv_rule_t));

        // 设置基本参数
        strncpy(rule->bind_addr, bind_addr, sizeof(rule->bind_addr) - 1);
        rule->bind_port = bind_port;

        // 设置存储引擎
        if (strcmp(engine_str, "sqlite") == 0) {
            rule->engine = POLY_MEMKV_ENGINE_SQLITE;
        } else if (strcmp(engine_str, "duckdb") == 0) {
            rule->engine = POLY_MEMKV_ENGINE_DUCKDB;
        } else {
            INFRA_LOG_ERROR("Invalid engine type at line %d: %s", line_num, engine_str);
            err = INFRA_ERROR_INVALID_PARAM;
            break;
        }

        // 设置可选参数
        if (matched >= 4 && strlen(db_path) > 0) {
            rule->db_path = strdup(db_path);
        } else {
            rule->db_path = strdup(":memory:");  // 默认使用内存数据库
        }

        if (matched >= 5 && strlen(plugin_path) > 0) {
            rule->plugin_path = strdup(plugin_path);
        }

        if (matched >= 6) {
            rule->max_memory = max_memory;
        }

        if (matched >= 7) {
            rule->enable_compression = enable_compression != 0;
        }

        if (matched >= 8) {
            rule->read_only = read_only != 0;
        }

        g_context.rule_count++;
    }

    fclose(fp);
    return err;
}

// 清理服务
static infra_error_t memkv_cleanup(void) {
    if (g_context.running) {
        memkv_stop();
    }

    // 清理规则
    for (int i = 0; i < g_context.rule_count; i++) {
        memkv_rule_t* rule = &g_context.rules[i];
        if (rule->db_path) {
            free(rule->db_path);
        }
        if (rule->plugin_path) {
            free(rule->plugin_path);
        }
    }
    g_context.rule_count = 0;
    
    return INFRA_OK;
}

// 启动服务
static infra_error_t memkv_start(void) {
    if (g_context.running) {
        INFRA_LOG_ERROR("Service already running");
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    // 创建线程池
    infra_thread_pool_config_t pool_config = {
        .min_threads = MEMKV_MIN_THREADS,
        .max_threads = MEMKV_MAX_THREADS,
        .queue_size = MEMKV_MAX_THREADS * 2
    };

    infra_error_t err = infra_thread_pool_create(&pool_config, &g_context.pool);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create thread pool: %d", err);
        return err;
    }

    // 创建所有监听器
    for (int i = 0; i < g_context.rule_count; i++) {
        memkv_rule_t* rule = &g_context.rules[i];
        infra_socket_t* listener = &g_context.listeners[i];

        // 创建监听socket
        infra_config_t config = {0};
        err = infra_net_create(listener, false, &config);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to create listener socket for rule %d: %d", i, err);
            goto cleanup;
        }

        // 设置地址重用
        err = infra_net_set_reuseaddr(*listener, true);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to set reuseaddr for rule %d: %d", i, err);
            goto cleanup;
        }

        // 绑定地址
        infra_net_addr_t addr = {
            .host = rule->bind_addr,
            .port = rule->bind_port
        };
        err = infra_net_bind(*listener, &addr);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to bind address for rule %d: %d", i, err);
            goto cleanup;
        }

        // 开始监听
        err = infra_net_listen(*listener);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to listen for rule %d: %d", i, err);
            goto cleanup;
        }

        // 设置非阻塞模式
        err = infra_net_set_nonblock(*listener, true);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to set nonblock for rule %d: %d", i, err);
            goto cleanup;
        }

        // 初始化poll事件
        g_context.polls[i].fd = infra_net_get_fd(*listener);
        g_context.polls[i].events = POLLIN;

        INFRA_LOG_INFO("Listening on %s:%d with %s engine", 
            rule->bind_addr, rule->bind_port,
            rule->engine == POLY_MEMKV_ENGINE_SQLITE ? "sqlite" : "duckdb");
    }

    g_context.running = true;

    // 主循环
    while (g_context.running) {
        // 等待事件
        int ready = poll(g_context.polls, g_context.rule_count, 1000);  // 1秒超时
        if (ready < 0) {
            if (errno == EINTR) {
                if (!g_context.running) break;
                continue;
            }
            INFRA_LOG_ERROR("Poll failed: %d", errno);
            continue;
        }

        if (ready == 0) {
            if (!g_context.running) break;
            continue;
        }

        // 检查每个监听器
        for (int i = 0; i < g_context.rule_count; i++) {
            if (!(g_context.polls[i].revents & POLLIN)) continue;

            // 接受连接
            infra_socket_t client = NULL;
            infra_net_addr_t client_addr = {0};
            err = infra_net_accept(g_context.listeners[i], &client, &client_addr);
            if (err != INFRA_OK) {
                if (err == INFRA_ERROR_WOULD_BLOCK) continue;
                INFRA_LOG_ERROR("Failed to accept connection: %d", err);
                continue;
            }

            INFRA_LOG_INFO("Accepted connection from %s:%d for rule %d", 
                client_addr.host, client_addr.port, i);

            // 创建连接结构
            memkv_conn_t* conn = malloc(sizeof(memkv_conn_t));
            if (!conn) {
                INFRA_LOG_ERROR("Failed to allocate connection");
                infra_net_close(client);
                continue;
            }

            // 初始化连接结构
            memset(conn, 0, sizeof(memkv_conn_t));
            conn->client = client;
            conn->rule = &g_context.rules[i];

            // 提交到线程池
            err = infra_thread_pool_submit(g_context.pool, handle_connection, conn);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to submit connection to thread pool: %d", err);
                infra_net_close(client);
                free(conn);
                continue;
            }
        }
    }

cleanup:
    // 停止服务
    g_context.running = false;

    // 清理监听器
    for (int i = 0; i < g_context.rule_count; i++) {
        if (g_context.listeners[i]) {
            infra_net_close(g_context.listeners[i]);
            g_context.listeners[i] = NULL;
        }
    }

    // 清理线程池
    if (g_context.pool) {
        infra_thread_pool_destroy(g_context.pool);
        g_context.pool = NULL;
    }

    return err;
}

// 停止服务
static infra_error_t memkv_stop(void) {
    if (!g_context.running) {
        INFRA_LOG_ERROR("Service not running");
        return INFRA_ERROR_NOT_READY;
    }
    
    // 设置停止标志
    g_context.running = false;
    
    // 等待服务线程退出
    INFRA_LOG_INFO("Stopping service...");
    
    return INFRA_OK;
}

// 检查服务是否运行
bool memkv_is_running(void) {
    return g_context.running;
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
            if (g_context.running) {
                INFRA_LOG_INFO("Service is running with %d rules:", g_context.rule_count);
                for (int j = 0; j < g_context.rule_count; j++) {
                    memkv_rule_t* rule = &g_context.rules[j];
                    INFRA_LOG_INFO("  Rule %d: %s:%d (%s)", j,
                        rule->bind_addr, rule->bind_port,
                        rule->engine == POLY_MEMKV_ENGINE_SQLITE ? "sqlite" : "duckdb");
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
        if (!config_path) {
            // 使用默认配置
            memkv_rule_t* rule = &g_context.rules[0];
            memset(rule, 0, sizeof(memkv_rule_t));
            strncpy(rule->bind_addr, "127.0.0.1", sizeof(rule->bind_addr) - 1);
            rule->bind_port = MEMKV_DEFAULT_PORT;
            rule->engine = POLY_MEMKV_ENGINE_SQLITE;
            rule->db_path = strdup(":memory:");  // 默认使用内存数据库
            rule->max_memory = 0;  // 无限制
            rule->enable_compression = false;
            rule->read_only = false;
            g_context.rule_count = 1;
            INFRA_LOG_INFO("Using default configuration");
        } else {
            err = load_config(config_path);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to load config: %d, using default configuration", err);
                // 使用默认配置
                memkv_rule_t* rule = &g_context.rules[0];
                memset(rule, 0, sizeof(memkv_rule_t));
                strncpy(rule->bind_addr, "127.0.0.1", sizeof(rule->bind_addr) - 1);
                rule->bind_port = MEMKV_DEFAULT_PORT;
                rule->engine = POLY_MEMKV_ENGINE_SQLITE;
                rule->db_path = strdup(":memory:");  // 默认使用内存数据库
                rule->max_memory = 0;  // 无限制
                rule->enable_compression = false;
                rule->read_only = false;
                g_context.rule_count = 1;
            }

            if (g_context.rule_count == 0) {
                INFRA_LOG_ERROR("No valid rules found in config, using default configuration");
                // 使用默认配置
                memkv_rule_t* rule = &g_context.rules[0];
                memset(rule, 0, sizeof(memkv_rule_t));
                strncpy(rule->bind_addr, "127.0.0.1", sizeof(rule->bind_addr) - 1);
                rule->bind_port = MEMKV_DEFAULT_PORT;
                rule->engine = POLY_MEMKV_ENGINE_SQLITE;
                rule->db_path = strdup(":memory:");  // 默认使用内存数据库
                rule->max_memory = 0;  // 无限制
                rule->enable_compression = false;
                rule->read_only = false;
                g_context.rule_count = 1;
            }
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
    uint32_t flags = 0;
    infra_error_t err = get_with_expiry(db, key, &value, &value_len, &flags);
    
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

// 实现 incr/decr 功能
static infra_error_t poly_memkv_counter_op(poly_memkv_db_t* db, const char* key, int64_t delta, int64_t* new_value) {
    if (!db || !key || !new_value) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    void* value = NULL;
    size_t value_len = 0;
    uint32_t flags = 0;
    infra_error_t err = get_with_expiry(db, key, &value, &value_len, &flags);
    
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
    err = set_with_expiry(db, key, buf, len, flags, 0);
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
