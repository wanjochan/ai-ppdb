#include "internal/peer/peer_service.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_memory.h"
#include "internal/poly/poly_memkv.h"
#include "internal/poly/poly_poll.h"
#define MEMKV_VERSION          "1.0.0"
#define MEMKV_MAX_ADDR_LEN 256
#define MEMKV_MAX_RULES 16
#define RING_BUFFER_SIZE 4096
#define MEMKV_BUFFER_SIZE      1048576  // 增加到 1MB
#define MEMKV_MAX_KEY_SIZE     250
#define MEMKV_MAX_VALUE_SIZE   (1024 * 1024)  // 1MB
#define MEMKV_MIN_THREADS      32
#define MEMKV_MAX_THREADS      512
#define MEMKV_DEFAULT_PORT     11211
#define POLY_OK 0
#define POLY_ERR_NOT_FOUND 1

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

// 环形缓冲区结构
typedef struct {
    char data[RING_BUFFER_SIZE];
    size_t read_pos;
    size_t write_pos;
    size_t bytes_available;
} ring_buffer_t;

// 连接上下文
typedef struct memkv_conn memkv_conn_t;
static void handle_set_command(memkv_conn_t* conn, const char* key, unsigned int flags, time_t exptime, size_t bytes);
static void handle_incr_decr_command(memkv_conn_t* conn, const char* key, const char* op, uint64_t delta);

// 连接上下文
typedef struct memkv_conn {
    infra_socket_t client;
    poly_memkv_db_t* store;
    memkv_rule_t* rule;
    bool active;
    ring_buffer_t rx_buf;
    ring_buffer_t tx_buf;
} memkv_conn_t;

// 服务上下文
typedef struct memkv_context {
    bool running;                        // 运行标志
    infra_thread_pool_t* pool;          // 线程池
    memkv_rule_t rules[MEMKV_MAX_RULES];// 规则列表
    int rule_count;                      // 规则数量
    infra_socket_t listeners[MEMKV_MAX_RULES]; // 监听socket列表
    poly_poll_context_t* poll_ctx;      // poll上下文
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
static void handle_connection(void* arg);  // 修改为 void 返回类型

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

// 函数前向声明
static void handle_client_command(memkv_conn_t* conn, const char* cmd);
static void handle_request(memkv_conn_t* conn, char* request, size_t len);
static void handle_binary_command(memkv_conn_t* conn, const char* cmd);
static void handle_text_command(memkv_conn_t* conn, const char* cmd);
static void handle_get_command(memkv_conn_t* conn, const char* key);
static void handle_incr_command(memkv_conn_t* conn, const char* key, const char* value);
static void send_binary_error_response(memkv_conn_t* conn, const char* error_msg);
static bool send_response(infra_socket_t sock, const char* response, size_t len);

// 环形缓冲区操作
static void ring_buffer_init(ring_buffer_t* buf) {
    memset(buf, 0, sizeof(*buf));
}

static size_t ring_buffer_write(ring_buffer_t* buf, const char* data, size_t len) {
    size_t free_space = RING_BUFFER_SIZE - buf->bytes_available;
    len = len > free_space ? free_space : len;
    
    size_t first_chunk = RING_BUFFER_SIZE - buf->write_pos;
    if (first_chunk >= len) {
        memcpy(buf->data + buf->write_pos, data, len);
        buf->write_pos = (buf->write_pos + len) % RING_BUFFER_SIZE;
    } else {
        memcpy(buf->data + buf->write_pos, data, first_chunk);
        memcpy(buf->data, data + first_chunk, len - first_chunk);
        buf->write_pos = len - first_chunk;
    }
    buf->bytes_available += len;
    return len;
}

// 处理客户端连接
static void handle_connection(void* arg) {
    poly_poll_handler_args_t* args = (poly_poll_handler_args_t*)arg;
    if (!args || !args->client) {
        INFRA_LOG_ERROR("Invalid connection parameters");
        if (args && args->client) infra_net_close(args->client);
        if (args) infra_free(args);
        return;
    }

    // 创建连接上下文
    memkv_conn_t* conn = (memkv_conn_t*)infra_malloc(sizeof(memkv_conn_t));
    if (!conn) {
        INFRA_LOG_ERROR("Failed to allocate connection context");
        infra_net_close(args->client);
        infra_free(args);
        return;
    }

    // 初始化连接上下文
    memset(conn, 0, sizeof(memkv_conn_t));
    conn->client = args->client;
    conn->rule = (memkv_rule_t*)args->user_data;
    if (!conn->rule) {
        INFRA_LOG_ERROR("Invalid rule in connection context");
        infra_net_close(conn->client);
        infra_free(conn);
        infra_free(args);
        return;
    }
    
    conn->active = true;
    ring_buffer_init(&conn->rx_buf);
    ring_buffer_init(&conn->tx_buf);

    // 创建存储引擎
    poly_memkv_config_t store_config = {0};  // 确保完全初始化
    store_config.url = conn->rule->db_path ? conn->rule->db_path : ":memory:";
    store_config.engine = conn->rule->engine;
    store_config.max_key_size = MEMKV_MAX_KEY_SIZE;
    store_config.max_value_size = MEMKV_MAX_VALUE_SIZE;
    store_config.memory_limit = conn->rule->max_memory;
    store_config.enable_compression = conn->rule->enable_compression;
    store_config.plugin_path = conn->rule->plugin_path;
    store_config.allow_fallback = true;
    store_config.read_only = conn->rule->read_only;

    INFRA_LOG_DEBUG("Creating store with config: engine=%d, url=%s", 
                    store_config.engine, store_config.url);

    // 创建存储引擎实例
    poly_memkv_db_t* store = NULL;
    infra_error_t err = poly_memkv_create(&store_config, &store);
    if (err != INFRA_OK || !store) {
        INFRA_LOG_ERROR("Failed to create store: %d", err);
        infra_net_close(conn->client);
        infra_free(conn);
        infra_free(args);
        return;
    }
    conn->store = store;

    // 设置非阻塞
    infra_net_set_nonblock(conn->client, true);

    char buffer[MEMKV_BUFFER_SIZE] = {0};
    size_t bytes_received = 0;

    INFRA_LOG_DEBUG("Connection handler started for client");

    while (conn->active) {
        // 接收数据
        err = infra_net_recv(conn->client, buffer, sizeof(buffer), &bytes_received);
        if (err != INFRA_OK) {
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                // 非阻塞模式下，暂时没有数据可读
                continue;
            }
            if (err == INFRA_ERROR_CLOSED) {
                INFRA_LOG_DEBUG("Client closed connection");
                break;
            }
            INFRA_LOG_ERROR("Failed to receive data: %d", err);
            break;
        }

        if (bytes_received == 0) {
            INFRA_LOG_DEBUG("Client closed connection");
            break;
        }

        // 处理命令
        buffer[bytes_received] = '\0';
        handle_request(conn, buffer, bytes_received);

        // 检查并发送响应缓冲区中的数据
        if (conn->tx_buf.bytes_available > 0) {
            size_t bytes_to_send = conn->tx_buf.bytes_available;
            size_t first_chunk = RING_BUFFER_SIZE - conn->tx_buf.read_pos;
            
            if (first_chunk >= bytes_to_send) {
                // 数据是连续的
                if (!send_response(conn->client, conn->tx_buf.data + conn->tx_buf.read_pos, bytes_to_send)) {
                    INFRA_LOG_ERROR("Failed to send response");
                    break;
                }
            } else {
                // 数据分两段发送
                if (!send_response(conn->client, conn->tx_buf.data + conn->tx_buf.read_pos, first_chunk)) {
                    INFRA_LOG_ERROR("Failed to send first chunk of response");
                    break;
                }
                if (!send_response(conn->client, conn->tx_buf.data, bytes_to_send - first_chunk)) {
                    INFRA_LOG_ERROR("Failed to send second chunk of response");
                    break;
                }
            }
            
            // 更新读取位置和可用字节数
            conn->tx_buf.read_pos = (conn->tx_buf.read_pos + bytes_to_send) % RING_BUFFER_SIZE;
            conn->tx_buf.bytes_available -= bytes_to_send;
        }
    }

    // 清理连接
    INFRA_LOG_DEBUG("Cleaning up connection");
    if (conn->store) {
        poly_memkv_destroy(conn->store);
        conn->store = NULL;
    }
    infra_net_close(conn->client);
    infra_free(conn);
    infra_free(args);
}

//-----------------------------------------------------------------------------
// Service Implementation
//-----------------------------------------------------------------------------

// 启动服务
static infra_error_t memkv_start(void) {
    if (g_context.running) {
        return INFRA_OK;
    }

    infra_error_t err = INFRA_OK;

    // 初始化监听器
    for (int i = 0; i < g_context.rule_count; i++) {
        memkv_rule_t* rule = &g_context.rules[i];

        // 创建监听器配置
        poly_poll_listener_t poll_listener = {0};
        strncpy(poll_listener.bind_addr, rule->bind_addr, POLY_MAX_ADDR_LEN - 1);
        poll_listener.bind_port = rule->bind_port;
        poll_listener.user_data = rule;

        // 添加到poll上下文
        err = poly_poll_add_listener(g_context.poll_ctx, &poll_listener);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to add listener: %d", err);
            goto cleanup;
        }

        INFRA_LOG_INFO("Listening on %s:%d", rule->bind_addr, rule->bind_port);
    }

    // 设置连接处理回调
    poly_poll_set_handler(g_context.poll_ctx, handle_connection);

    // 启动poll
    err = poly_poll_start(g_context.poll_ctx);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to start poll: %d", err);
        goto cleanup;
    }

    g_context.running = true;
    return INFRA_OK;

cleanup:
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
static void handle_request(memkv_conn_t* conn, char* request, size_t len) {
    if (!conn || !request || len == 0) {
        INFRA_LOG_ERROR("Invalid request parameters");
        return;
    }

    // 检查缓冲区是否有足够空间
    if (RING_BUFFER_SIZE - conn->tx_buf.bytes_available < 1024) {
        INFRA_LOG_ERROR("Response buffer is nearly full");
        ring_buffer_write(&conn->tx_buf, "SERVER_ERROR buffer full\r\n", 25);
        return;
    }

    // 清理请求末尾的换行符
    while (len > 0 && (request[len-1] == '\r' || request[len-1] == '\n')) {
        request[--len] = '\0';
    }

    if (len == 0) {
        INFRA_LOG_DEBUG("Empty request received");
        return;
    }

    char* saveptr;
    char* cmd = strtok_r(request, " \r\n", &saveptr);
    if (!cmd) {
        INFRA_LOG_ERROR("Invalid command format");
        ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
        return;
    }

    // 转换命令为小写
    for (char* p = cmd; *p; ++p) *p = tolower(*p);

    INFRA_LOG_DEBUG("Processing command: %s", cmd);

    if (strcmp(cmd, "get") == 0) {
        char* key = strtok_r(NULL, " \r\n", &saveptr);
        if (!key) {
            INFRA_LOG_ERROR("GET: Missing key");
            ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
            return;
        }

        void* value;
        size_t value_len;
        
        infra_error_t err = poly_memkv_get(conn->store, key, &value, &value_len);
        if (err == INFRA_OK) {
            char header[256];
            int header_len = snprintf(header, sizeof(header), "VALUE %s 0 %zu\r\n", key, value_len);
            if (header_len < 0 || header_len >= sizeof(header)) {
                INFRA_LOG_ERROR("GET: Header buffer overflow");
                if (value) infra_free(value);
                ring_buffer_write(&conn->tx_buf, "SERVER_ERROR header overflow\r\n", 29);
                return;
            }

            // 检查缓冲区空间是否足够
            if (RING_BUFFER_SIZE - conn->tx_buf.bytes_available < header_len + value_len + 7) {
                INFRA_LOG_ERROR("GET: Response buffer overflow");
                if (value) infra_free(value);
                ring_buffer_write(&conn->tx_buf, "SERVER_ERROR buffer overflow\r\n", 29);
                return;
            }

            ring_buffer_write(&conn->tx_buf, header, header_len);
            ring_buffer_write(&conn->tx_buf, value, value_len);
            ring_buffer_write(&conn->tx_buf, "\r\nEND\r\n", 7);
            if (value) infra_free(value);
        } else {
            INFRA_LOG_DEBUG("GET: Key not found: %s", key);
            ring_buffer_write(&conn->tx_buf, "END\r\n", 5);
        }
    } else if (strcmp(cmd, "set") == 0) {
        char* key = strtok_r(NULL, " ", &saveptr);
        char* flags_str = strtok_r(NULL, " ", &saveptr);
        char* expiry_str = strtok_r(NULL, " ", &saveptr);
        char* bytes_str = strtok_r(NULL, " \r\n", &saveptr);

        if (!key || !flags_str || !expiry_str || !bytes_str) {
            INFRA_LOG_ERROR("SET: Invalid parameters");
            ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
            return;
        }

        uint32_t flags = atoi(flags_str);
        time_t expiry = atoi(expiry_str);
        size_t bytes = atoi(bytes_str);

        if (bytes > MEMKV_MAX_VALUE_SIZE) {
            INFRA_LOG_ERROR("SET: Value too large: %zu", bytes);
            ring_buffer_write(&conn->tx_buf, "SERVER_ERROR value too large\r\n", 29);
            return;
        }

        char* value = infra_malloc(bytes + 2);  // +2 for \r\n
        if (!value) {
            INFRA_LOG_ERROR("SET: Failed to allocate memory for value");
            ring_buffer_write(&conn->tx_buf, "SERVER_ERROR out of memory\r\n", 27);
            return;
        }

        size_t received = 0;
        infra_error_t err = infra_net_recv(conn->client, value, bytes + 2, &received);
        if (err != INFRA_OK || received != bytes + 2 || value[bytes] != '\r' || value[bytes + 1] != '\n') {
            INFRA_LOG_ERROR("SET: Failed to receive value data");
            infra_free(value);
            ring_buffer_write(&conn->tx_buf, "CLIENT_ERROR bad data chunk\r\n", 29);
            return;
        }

        err = poly_memkv_set(conn->store, key, value, bytes);
        if (err == INFRA_OK && expiry > 0) {
            err = poly_memkv_expire(conn->store, key, expiry);
        }

        infra_free(value);
        ring_buffer_write(&conn->tx_buf, err == INFRA_OK ? "STORED\r\n" : "NOT_STORED\r\n", 
                         err == INFRA_OK ? 8 : 12);
    } else if (strcmp(cmd, "delete") == 0) {
        char* key = strtok_r(NULL, " \r\n", &saveptr);
        if (!key) {
            INFRA_LOG_ERROR("DELETE: Missing key");
            ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
            return;
        }

        infra_error_t err = poly_memkv_del(conn->store, key);
        if (err == INFRA_OK) {
            INFRA_LOG_DEBUG("DELETE: Key deleted successfully: %s", key);
            ring_buffer_write(&conn->tx_buf, "DELETED\r\n", 9);
        } else {
            INFRA_LOG_DEBUG("DELETE: Key not found: %s", key);
            ring_buffer_write(&conn->tx_buf, "NOT_FOUND\r\n", 11);
        }
    } else {
        INFRA_LOG_ERROR("Unknown command: %s", cmd);
        ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
    }
}

// 处理客户端命令
static void handle_client_command(memkv_conn_t* conn, const char* cmd) {
    // 检查命令是否为空
    if (!cmd || strlen(cmd) == 0) {
        INFRA_LOG_DEBUG("Empty command received");
        ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
        return;
    }

    // 打印收到的命令（去除末尾的\r\n）
    char debug_cmd[1024] = {0};
    strncpy(debug_cmd, cmd, sizeof(debug_cmd) - 1);
    char* end = strstr(debug_cmd, "\r\n");
    if (end) *end = '\0';
    INFRA_LOG_DEBUG("Received command: [%s]", debug_cmd);

    // 检查是否为二进制协议（二进制协议的第一个字节是 0x80）
    if ((unsigned char)cmd[0] == 0x80) {
        INFRA_LOG_DEBUG("Handling binary command");
        handle_binary_command(conn, cmd);
    } else {
        INFRA_LOG_DEBUG("Handling text command");
        handle_text_command(conn, cmd);
    }
}

static void handle_binary_command(memkv_conn_t* conn, const char* cmd) {
    if (strncmp(cmd, "GET", 3) == 0) {
        const char* key = cmd + 4;
        handle_get_command(conn, key);
    } else if (strncmp(cmd, "INCR", 4) == 0) {
        const char* key = cmd + 5;
        const char* value = strchr(key, ' ');
        if (value) {
            value++;
            handle_incr_command(conn, key, value);
        } else {
            send_binary_error_response(conn, "INVALID_INCR_COMMAND");
        }
    } else {
        send_binary_error_response(conn, "UNKNOWN_COMMAND");
    }
}

static void handle_text_command(memkv_conn_t* conn, const char* cmd) {
    char op[16] = {0};
    char key[256] = {0};
    
    // 移除命令末尾的\r\n
    char clean_cmd[1024] = {0};
    strncpy(clean_cmd, cmd, sizeof(clean_cmd) - 1);
    char* end = strstr(clean_cmd, "\r\n");
    if (end) {
        *end = '\0';
    }
    
    if (sscanf(clean_cmd, "%15s", op) < 1) {
        INFRA_LOG_ERROR("Failed to parse command");
        ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
        return;
    }

    INFRA_LOG_DEBUG("Processing command: %s", op);

    if (strcmp(op, "get") == 0) {
        if (sscanf(clean_cmd, "%*s %255s", key) < 1) {
            INFRA_LOG_ERROR("GET: Missing key");
            ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
            return;
        }

        INFRA_LOG_DEBUG("GET: key=[%s]", key);

        void* value;
        size_t value_len;
        if (poly_memkv_get(conn->store, key, &value, &value_len) == POLY_OK) {
            char header[256];
            snprintf(header, sizeof(header), "VALUE %s 0 %zu\r\n", key, value_len);
            INFRA_LOG_DEBUG("GET: Found value, sending header: [%s]", header);
            
            ring_buffer_write(&conn->tx_buf, header, strlen(header));
            ring_buffer_write(&conn->tx_buf, value, value_len);
            ring_buffer_write(&conn->tx_buf, "\r\n", 2);
            free(value);
        } else {
            INFRA_LOG_DEBUG("GET: Key not found: [%s]", key);
        }
        ring_buffer_write(&conn->tx_buf, "END\r\n", 5);
    } else if (strcmp(op, "set") == 0) {
        unsigned int flags;
        unsigned int exptime;
        unsigned int bytes;
        
        // 解析SET命令的头部
        if (sscanf(clean_cmd, "%*s %255s %u %u %u", key, &flags, &exptime, &bytes) < 4) {
            INFRA_LOG_ERROR("SET: Invalid format");
            ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
            return;
        }

        INFRA_LOG_DEBUG("SET: key=[%s], flags=%u, exptime=%u, bytes=%u", 
                       key, flags, exptime, bytes);

        // 寻找数据部分
        const char* value_start = strstr(cmd, "\r\n");
        if (!value_start || value_start[1] != '\n') {
            INFRA_LOG_ERROR("SET: Missing data part");
            ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
            return;
        }
        value_start += 2;  // 跳过\r\n

        // 设置值
        if (poly_memkv_set(conn->store, key, value_start, bytes) == POLY_OK) {
            INFRA_LOG_DEBUG("SET: Successfully stored value");
            ring_buffer_write(&conn->tx_buf, "STORED\r\n", 8);
        } else {
            INFRA_LOG_ERROR("SET: Failed to store value");
            ring_buffer_write(&conn->tx_buf, "NOT_STORED\r\n", 12);
        }
    } else if (strcmp(op, "delete") == 0) {
        if (sscanf(clean_cmd, "%*s %255s", key) < 1) {
            INFRA_LOG_ERROR("DELETE: Missing key");
            ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
            return;
        }

        INFRA_LOG_DEBUG("DELETE: key=[%s]", key);

        if (poly_memkv_del(conn->store, key) == POLY_OK) {
            INFRA_LOG_DEBUG("DELETE: Key deleted successfully");
            ring_buffer_write(&conn->tx_buf, "DELETED\r\n", 9);
        } else {
            INFRA_LOG_DEBUG("DELETE: Key not found");
            ring_buffer_write(&conn->tx_buf, "NOT_FOUND\r\n", 11);
        }
    } else if (strcmp(op, "incr") == 0 || strcmp(op, "decr") == 0) {
        unsigned long delta;
        if (sscanf(clean_cmd, "%*s %255s %lu", key, &delta) < 2) {
            INFRA_LOG_ERROR("%s: Invalid format", op);
            ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
            return;
        }

        INFRA_LOG_DEBUG("%s: key=[%s], delta=%lu", op, key, delta);

        void* stored_value;
        size_t value_len;
        if (poly_memkv_get(conn->store, key, &stored_value, &value_len) != POLY_OK) {
            INFRA_LOG_DEBUG("%s: Key not found", op);
            ring_buffer_write(&conn->tx_buf, "NOT_FOUND\r\n", 11);
            return;
        }

        char* endptr;
        unsigned long current = strtoul((char*)stored_value, &endptr, 10);
        if (*endptr != '\0') {
            INFRA_LOG_ERROR("%s: Non-numeric value: [%s]", op, (char*)stored_value);
            free(stored_value);
            ring_buffer_write(&conn->tx_buf, "CLIENT_ERROR cannot increment or decrement non-numeric value\r\n", 60);
            return;
        }

        unsigned long result;
        if (strcmp(op, "incr") == 0) {
            result = current + delta;
            INFRA_LOG_DEBUG("INCR: %lu + %lu = %lu", current, delta, result);
        } else {
            result = (current > delta) ? (current - delta) : 0;
            INFRA_LOG_DEBUG("DECR: %lu - %lu = %lu", current, delta, result);
        }

        char new_value[32];
        int new_len = snprintf(new_value, sizeof(new_value), "%lu", result);

        if (poly_memkv_set(conn->store, key, new_value, new_len) != POLY_OK) {
            free(stored_value);
            ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
            return;
        }

        free(stored_value);
        char response[32];
        int response_len = snprintf(response, sizeof(response), "%lu\r\n", result);
        INFRA_LOG_DEBUG("%s: Sending response: [%.*s]", op, response_len-2, response);
        ring_buffer_write(&conn->tx_buf, response, response_len);
    } else if (strcmp(op, "flush_all") == 0) {
        INFRA_LOG_DEBUG("FLUSH_ALL: Clearing all data");
        
        if (poly_memkv_flush_all(conn->store) == POLY_OK) {
            INFRA_LOG_DEBUG("FLUSH_ALL: Successfully cleared all data");
            ring_buffer_write(&conn->tx_buf, "OK\r\n", 4);
        } else {
            INFRA_LOG_ERROR("FLUSH_ALL: Failed to clear data");
            ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
        }
    } else {
        INFRA_LOG_ERROR("Unknown command: [%s]", op);
        ring_buffer_write(&conn->tx_buf, "ERROR\r\n", 7);
    }
}

static void handle_get_command(memkv_conn_t* conn, const char* key) {
    void* value;
    size_t value_len;
    
    if (poly_memkv_get(conn->store, key, &value, &value_len) == POLY_OK) {
        char header[256];
        snprintf(header, sizeof(header), "VALUE %s 0 %zu\r\n", key, value_len);
        INFRA_LOG_DEBUG("GET: Found value, sending header: [%s]", header);
        
        ring_buffer_write(&conn->tx_buf, header, strlen(header));
        ring_buffer_write(&conn->tx_buf, value, value_len);
        ring_buffer_write(&conn->tx_buf, "\r\n", 2);
        free(value);
    } else {
        INFRA_LOG_DEBUG("GET: Key not found: [%s]", key);
    }
    ring_buffer_write(&conn->tx_buf, "END\r\n", 5);
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

// 初始化服务
static infra_error_t memkv_init(const infra_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 初始化上下文
    memset(&g_context, 0, sizeof(g_context));

    // 创建 poly_poll 配置
    poly_poll_config_t poll_config = {
        .min_threads = MEMKV_MIN_THREADS,
        .max_threads = MEMKV_MAX_THREADS,
        .queue_size = 1024,
        .max_listeners = MEMKV_MAX_RULES
    };

    // 分配 poll 上下文
    g_context.poll_ctx = (poly_poll_context_t*)infra_malloc(sizeof(poly_poll_context_t));
    if (!g_context.poll_ctx) {
        INFRA_LOG_ERROR("Failed to allocate poll context");
        return INFRA_ERROR_NO_MEMORY;
    }

    // 初始化poll上下文
    infra_error_t err = poly_poll_init(g_context.poll_ctx, &poll_config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to init poll context: %d", err);
        infra_free(g_context.poll_ctx);
        g_context.poll_ctx = NULL;
        return err;
    }

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
    if (g_context.running) {
        memkv_stop();
    }

    // 销毁 poly_poll 上下文
    if (g_context.poll_ctx) {
        poly_poll_cleanup(g_context.poll_ctx);
        g_context.poll_ctx = NULL;
    }

    return INFRA_OK;
}
