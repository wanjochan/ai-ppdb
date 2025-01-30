// #include "internal/peer/peer_rinetd.h"

//rinet: tiny ports forwarding

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_net.h"
#include "internal/peer/peer_service.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define RINETD_BUFFER_SIZE 16384        // 转发缓冲区大小
#define RINETD_MAX_ADDR_LEN 256        // 地址最大长度
#define RINETD_MAX_PATH_LEN 256        // 路径最大长度
#define RINETD_MIN_THREADS 32           // 最小线程数
#define RINETD_MAX_THREADS 512         // 最大线程数
#define RINETD_MAX_RULES 128           // 最大规则数
#define RINETD_MAX_EVENTS 1024         // 最大事件数

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// 转发规则
typedef struct {
    char src_addr[RINETD_MAX_ADDR_LEN];  // 源地址
    int src_port;                        // 源端口
    char dst_addr[RINETD_MAX_ADDR_LEN];  // 目标地址
    int dst_port;                        // 目标端口
} rinetd_rule_t;

// 监听线程参数
typedef struct {
    rinetd_rule_t* rule;                // 关联的规则
    infra_socket_t listener;            // 监听socket
} listener_thread_param_t;

// 转发连接
typedef struct {
    infra_socket_t client;              // 客户端socket
    infra_socket_t server;              // 服务端socket
    rinetd_rule_t* rule;                // 关联的规则
    char buffer[RINETD_BUFFER_SIZE];    // 转发缓冲区
} rinetd_conn_t;

// 连接会话
typedef struct {
    infra_socket_t client;              // 客户端socket
    infra_socket_t server;              // 服务器socket
    volatile bool c2s_failed;           // 客户端到服务器方向是否失败
    volatile bool s2c_failed;           // 服务器到客户端方向是否失败
} rinetd_session_t;

// 服务上下文
typedef struct {
    bool running;                        // 服务是否运行
    char config_path[RINETD_MAX_PATH_LEN]; // 配置文件路径
    rinetd_rule_t* rules;               // 转发规则数组
    int rule_count;                      // 规则数量
    infra_thread_pool_t* pool;          // 线程池
    infra_socket_t* listeners;          // 监听socket数组
    infra_thread_t* listener_threads;    // 监听线程数组
    infra_mutex_t* mutex;               // 全局互斥锁
    rinetd_session_t* active_sessions;  // 活跃会话数组
    int session_count;                  // 会话数量
    struct pollfd* polls;               // poll 事件数组
    int poll_count;                     // poll 事件数量
} rinetd_context_t;

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

// 声明服务实例
// extern peer_service_t g_rinetd_service;

extern void bzero(void* s, size_t n);

//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------

// Service interface functions
static infra_error_t rinetd_init(const infra_config_t* config);
static infra_error_t rinetd_cleanup(void);
static infra_error_t rinetd_start(void);
static infra_error_t rinetd_stop(void);
static bool rinetd_is_running(void);
static infra_error_t rinetd_cmd_handler(int argc, char** argv);

// Configuration functions
static infra_error_t rinetd_load_config(const char* path);
static infra_error_t rinetd_save_config(const char* path);

//-----------------------------------------------------------------------------
// Command Line Options
//-----------------------------------------------------------------------------

const poly_cmd_option_t rinetd_options[] = {
    {"config", "Config file path", true},
    {"start", "Start the service", false},
    {"stop", "Stop the service", false},
    {"status", "Show service status", false},
};

const int rinetd_option_count = sizeof(rinetd_options) / sizeof(rinetd_options[0]);

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

// 服务实例
peer_service_t g_rinetd_service = {
    .config = {
        .name = "rinetd",
        .type = SERVICE_TYPE_RINETD,
        .options = rinetd_options,
        .option_count = rinetd_option_count,
        .config = NULL
    },
    .state = SERVICE_STATE_STOPPED,
    .init = rinetd_init,
    .cleanup = rinetd_cleanup,
    .start = rinetd_start,
    .stop = rinetd_stop,
    .is_running = rinetd_is_running,
    .cmd_handler = rinetd_cmd_handler
};

// 服务上下文
static struct {
    bool running;
    rinetd_rule_t rules[RINETD_MAX_RULES];  // 规则数组
    int rule_count;                          // 当前规则数量
    infra_socket_t listeners[RINETD_MAX_RULES];  // 每个规则对应一个监听器
    infra_thread_pool_t* pool;
    struct pollfd* polls;                    // poll事件数组
    int poll_count;                          // poll事件数量
} g_context = {0};

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

// 转发数据
static infra_error_t forward_data(infra_socket_t src, infra_socket_t dst, char* buffer, const char* direction) {
    size_t bytes_received = 0;
    size_t bytes_sent = 0;
    size_t total_sent = 0;
    int retry_count = 0;
    const int MAX_RETRIES = 3;
    
    // 接收数据
    infra_error_t err = infra_net_recv(src, buffer, RINETD_BUFFER_SIZE, &bytes_received);
    if (err != INFRA_OK) {
        if (err == INFRA_ERROR_TIMEOUT || err == INFRA_ERROR_WOULD_BLOCK) {
            return INFRA_ERROR_TIMEOUT;
        }
        INFRA_LOG_ERROR("Failed to receive data: %d", err);
        return err;
    }
    
    if (bytes_received == 0) {
        INFRA_LOG_DEBUG("Peer closed connection");
        return INFRA_ERROR_CLOSED;
    }

    // 循环发送直到所有数据都发送完
    while (total_sent < bytes_received && retry_count < MAX_RETRIES) {
        err = infra_net_send(dst, buffer + total_sent, bytes_received - total_sent, &bytes_sent);
        if (err != INFRA_OK) {
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                // 发送缓冲区满，短暂等待后重试
                infra_sleep(10);  // 等待10ms
                retry_count++;
                continue;
            }
            if (err == INFRA_ERROR_TIMEOUT) {
                retry_count++;
                continue;
            }
            INFRA_LOG_ERROR("Failed to send data: %d", err);
            return err;
        }
        
        if (bytes_sent == 0) {
            retry_count++;
            if (retry_count >= MAX_RETRIES) {
                INFRA_LOG_ERROR("Send failed after %d retries", MAX_RETRIES);
                return INFRA_ERROR_IO;
            }
            continue;
        }

        total_sent += bytes_sent;
        retry_count = 0;  // 重置重试计数
    }

    if (total_sent < bytes_received) {
        INFRA_LOG_ERROR("%s: Failed to send all data after %d retries", direction, MAX_RETRIES);
        return INFRA_ERROR_IO;
    }

    INFRA_LOG_DEBUG("%s: %zu bytes forwarded", direction, total_sent);
    return INFRA_OK;
}

// 处理单个连接
static void* handle_connection(void* arg) {
    rinetd_conn_t* conn = (rinetd_conn_t*)arg;
    if (!conn) {
        return NULL;
    }

    INFRA_LOG_DEBUG("Started forwarding: %s:%d -> %s:%d",
        conn->rule->src_addr, conn->rule->src_port,
        conn->rule->dst_addr, conn->rule->dst_port);

    // 设置非阻塞模式
    infra_net_set_nonblock(conn->client, true);
    infra_net_set_nonblock(conn->server, true);

    // 设置socket超时（30秒）
    infra_net_set_timeout(conn->client, 30000);  // 30秒 = 30000毫秒
    infra_net_set_timeout(conn->server, 30000);

    // 连接状态
    bool client_closed = false;
    bool server_closed = false;
    int idle_count = 0;
    const int MAX_IDLE = 600;  // 最大空闲次数（约60秒）

    // 创建 poll 事件数组
    struct pollfd polls[2] = {0};
    polls[0].fd = infra_net_get_fd(conn->client);
    polls[1].fd = infra_net_get_fd(conn->server);

    while (g_context.running && !client_closed && !server_closed && idle_count < MAX_IDLE) {
        // 设置 poll 事件
        polls[0].events = POLLIN;
        polls[1].events = POLLIN;

        // 等待事件
        int ready = poll(polls, 2, 100);  // 100ms timeout
        if (ready < 0) {
            if (errno == EINTR) continue;
            INFRA_LOG_ERROR("Poll failed: %d", errno);
            break;
        }
        
        if (ready == 0) {
            idle_count++;
            continue;
        }
        
        idle_count = 0;  // 重置空闲计数

        // 处理客户端到服务器的数据
        if (!client_closed && (polls[0].revents & POLLIN)) {
            infra_error_t err = forward_data(conn->client, conn->server, conn->buffer, "C->S");
            if (err == INFRA_ERROR_CLOSED) {
                INFRA_LOG_DEBUG("Client closed connection");
                client_closed = true;
            } else if (err != INFRA_OK && err != INFRA_ERROR_TIMEOUT) {
                INFRA_LOG_ERROR("Forward C->S failed: %d", err);
                break;
            }
        }

        // 处理服务器到客户端的数据
        if (!server_closed && (polls[1].revents & POLLIN)) {
            infra_error_t err = forward_data(conn->server, conn->client, conn->buffer, "S->C");
            if (err == INFRA_ERROR_CLOSED) {
                INFRA_LOG_DEBUG("Server closed connection");
                server_closed = true;
            } else if (err != INFRA_OK && err != INFRA_ERROR_TIMEOUT) {
                INFRA_LOG_ERROR("Forward S->C failed: %d", err);
                break;
            }
        }
    }

    // 清理连接
    INFRA_LOG_DEBUG("Cleaning up connection after %s", 
        idle_count >= MAX_IDLE ? "idle timeout" : "normal close");
    
    if (conn->client) {
        infra_net_shutdown(conn->client, INFRA_NET_SHUTDOWN_BOTH);
        infra_net_close(conn->client);
    }
    if (conn->server) {
        infra_net_shutdown(conn->server, INFRA_NET_SHUTDOWN_BOTH);
        infra_net_close(conn->server);
    }
    free(conn);
    return NULL;
}

//-----------------------------------------------------------------------------
// Core Functions Implementation
//-----------------------------------------------------------------------------

infra_error_t rinetd_init(const infra_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    memset(&g_context, 0, sizeof(g_context));

    // 创建线程池
    infra_thread_pool_config_t pool_config = {
        .min_threads = RINETD_MIN_THREADS,
        .max_threads = RINETD_MAX_THREADS,
        .queue_size = RINETD_MAX_THREADS * 2
    };

    infra_error_t err = infra_thread_pool_create(&pool_config, &g_context.pool);
    if (err != INFRA_OK) {
        return err;
    }

    return INFRA_OK;
}

infra_error_t rinetd_cleanup(void) {
    if (g_context.running) {
        rinetd_stop();
    }

    if (g_context.pool) {
        infra_thread_pool_destroy(g_context.pool);
        g_context.pool = NULL;
    }

    // 清理所有监听器
    for (int i = 0; i < g_context.rule_count; i++) {
        if (g_context.listeners[i]) {
            infra_net_close(g_context.listeners[i]);
            g_context.listeners[i] = NULL;
        }
    }
    g_context.rule_count = 0;

    // 清理 poll 事件数组
    if (g_context.polls) {
        free(g_context.polls);
        g_context.polls = NULL;
    }
    g_context.poll_count = 0;

    return INFRA_OK;
}

static infra_error_t create_listener(int rule_index) {
    if (rule_index < 0 || rule_index >= g_context.rule_count) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 创建监听socket
    infra_socket_t listener = NULL;
    infra_config_t config = {0};
    infra_error_t err = infra_net_create(&listener, false, &config);
    if (err != INFRA_OK) {
        return err;
    }

    // 设置地址重用
    err = infra_net_set_reuseaddr(listener, true);
    if (err != INFRA_OK) {
        infra_net_close(listener);
        return err;
    }

    // 绑定地址
    infra_net_addr_t addr = {0};
    addr.host = g_context.rules[rule_index].src_addr;
    addr.port = g_context.rules[rule_index].src_port;
    err = infra_net_bind(listener, &addr);
    if (err != INFRA_OK) {
        infra_net_close(listener);
        return err;
    }

    // 开始监听
    err = infra_net_listen(listener);
    if (err != INFRA_OK) {
        infra_net_close(listener);
        return err;
    }

    g_context.listeners[rule_index] = listener;
    return INFRA_OK;
}

infra_error_t rinetd_start(void) {
    if (g_context.running) {
        return INFRA_ERROR_BUSY;
    }

    // 检查是否有规则
    if (g_context.rule_count == 0) {
        INFRA_LOG_ERROR("No rules configured");
        return INFRA_ERROR_INVALID_STATE;
    }

    // 为每个规则创建监听器
    for (int i = 0; i < g_context.rule_count; i++) {
        infra_error_t err = create_listener(i);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to create listener for rule %d: %d", i, err);
            // 清理已创建的监听器
            for (int j = 0; j < i; j++) {
                infra_net_close(g_context.listeners[j]);
                g_context.listeners[j] = NULL;
            }
            return err;
        }
    }

    // 初始化 poll 事件数组
    g_context.polls = malloc(sizeof(struct pollfd) * g_context.rule_count);
    if (!g_context.polls) {
        INFRA_LOG_ERROR("Failed to allocate poll events");
        return INFRA_ERROR_NO_MEMORY;
    }

    // 设置运行标志
    g_context.running = true;

    // 在前台运行
    INFRA_LOG_INFO("Starting rinetd service in foreground with %d rules", g_context.rule_count);
    
    // 初始化 poll 事件
    for (int i = 0; i < g_context.rule_count; i++) {
        g_context.polls[i].fd = infra_net_get_fd(g_context.listeners[i]);
        g_context.polls[i].events = POLLIN;
    }
    g_context.poll_count = g_context.rule_count;

    while (g_context.running) {
        // 等待连接
        int ready = poll(g_context.polls, g_context.poll_count, 1000);  // 1秒超时
        if (ready < 0) {
            if (errno == EINTR) continue;
            INFRA_LOG_ERROR("Poll failed: %d", errno);
            break;
        }
        if (ready == 0) continue;  // 超时，继续循环

        // 检查每个监听器
        for (int i = 0; i < g_context.rule_count; i++) {
            if (!(g_context.polls[i].revents & POLLIN)) continue;

            // 接受连接
            infra_socket_t client = NULL;
            infra_net_addr_t client_addr = {0};
            infra_error_t err = infra_net_accept(g_context.listeners[i], &client, &client_addr);
            if (err != INFRA_OK) {
                if (err == INFRA_ERROR_WOULD_BLOCK) continue;
                INFRA_LOG_ERROR("Failed to accept connection: %d", err);
                continue;
            }

            INFRA_LOG_INFO("Accepted connection from %s:%d for rule %d", 
                client_addr.host, client_addr.port, i);

            // 创建服务器连接
            infra_socket_t server = NULL;
            infra_config_t config = {0};
            err = infra_net_create(&server, false, &config);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to create server socket: %d", err);
                infra_net_close(client);
                continue;
            }

            // 连接到目标服务器
            infra_net_addr_t addr = {0};
            addr.host = g_context.rules[i].dst_addr;
            addr.port = g_context.rules[i].dst_port;
            err = infra_net_connect(&addr, &server, &config);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to connect to server: %d", err);
                infra_net_close(client);
                infra_net_close(server);
                continue;
            }

            INFRA_LOG_INFO("Connected to server %s:%d for rule %d", 
                addr.host, addr.port, i);

            // 创建连接结构
            rinetd_conn_t* conn = malloc(sizeof(rinetd_conn_t));
            if (!conn) {
                INFRA_LOG_ERROR("Failed to allocate connection");
                infra_net_close(client);
                infra_net_close(server);
                continue;
            }

            conn->client = client;
            conn->server = server;
            conn->rule = &g_context.rules[i];

            // 提交到线程池
            err = infra_thread_pool_submit(g_context.pool, handle_connection, conn);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to submit connection to thread pool: %d", err);
                infra_net_close(client);
                infra_net_close(server);
                free(conn);
                continue;
            }
        }
    }

    return INFRA_OK;
}

infra_error_t rinetd_stop(void) {
    if (!g_context.running) {
        return INFRA_OK;
    }

    g_context.running = false;
    
    // 清理所有监听器
    for (int i = 0; i < g_context.rule_count; i++) {
        if (g_context.listeners[i]) {
            infra_net_close(g_context.listeners[i]);
            g_context.listeners[i] = NULL;
        }
    }
    g_context.rule_count = 0;

    return INFRA_OK;
}

bool rinetd_is_running(void) {
    return g_context.running;
}

//-----------------------------------------------------------------------------
// Command Handler
//-----------------------------------------------------------------------------

infra_error_t rinetd_cmd_handler(int argc, char** argv) {
    if (argc < 1) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Initialize service
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    infra_error_t err = rinetd_init(&config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize rinetd service: %d", err);
        return err;
    }

    // Parse command line
    bool should_start = false;
    const char* config_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--start") == 0) {
            should_start = true;
        } else if (strcmp(argv[i], "--stop") == 0) {
            return rinetd_stop();
        } else if (strcmp(argv[i], "--status") == 0) {
            infra_printf("Service is %s\n", 
                rinetd_is_running() ? "running" : "stopped");
            return INFRA_OK;
        } else if (strncmp(argv[i], "--config=", 9) == 0) {
            config_path = argv[i] + 9;
        } else if (strcmp(argv[i], "--config") == 0) {
            if (++i >= argc) {
                INFRA_LOG_ERROR("Missing config file path");
                return INFRA_ERROR_INVALID_PARAM;
            }
            config_path = argv[i];
        }
    }

    // Load config if specified
    if (config_path) {
        INFRA_LOG_DEBUG("Loading config file: %s", config_path);
        err = rinetd_load_config(config_path);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to load config file: %d", err);
            return err;
        }
    } else if (should_start) {
        INFRA_LOG_ERROR("Config file is required to start the service");
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Start service if requested
    if (should_start) {
        INFRA_LOG_DEBUG("Starting rinetd service");
        err = rinetd_start();
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to start rinetd service: %d", err);
            return err;
        }
        INFRA_LOG_INFO("Rinetd service started successfully");
    }

    return INFRA_OK;
}

infra_error_t rinetd_load_config(const char* path) {
    if (!path) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 重置规则计数
    g_context.rule_count = 0;

    // 尝试加载配置文件
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        INFRA_LOG_ERROR("Failed to open config file: %s", path);
        return INFRA_ERROR_IO;
    }

    // 解析配置文件
    char line[256];
    int line_num = 0;
    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        
        // 跳过注释和空行
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        // 检查规则数量限制
        if (g_context.rule_count >= RINETD_MAX_RULES) {
            INFRA_LOG_ERROR("Too many rules (max: %d)", RINETD_MAX_RULES);
            fclose(fp);
            return INFRA_ERROR_NO_MEMORY;
        }

        // 解析规则
        char src_addr[RINETD_MAX_ADDR_LEN] = {0};
        char dst_addr[RINETD_MAX_ADDR_LEN] = {0};
        int src_port = 0;
        int dst_port = 0;

        int matched = sscanf(line, "%s %d %s %d", src_addr, &src_port, dst_addr, &dst_port);
        if (matched != 4) {
            INFRA_LOG_WARN("Invalid rule format at line %d: %s", line_num, line);
            continue;
        }

        // 验证端口范围
        if (src_port <= 0 || src_port > 65535 || dst_port <= 0 || dst_port > 65535) {
            INFRA_LOG_WARN("Invalid port number at line %d", line_num);
            continue;
        }

        // 验证地址长度
        if (strlen(src_addr) >= RINETD_MAX_ADDR_LEN || strlen(dst_addr) >= RINETD_MAX_ADDR_LEN) {
            INFRA_LOG_WARN("Address too long at line %d", line_num);
            continue;
        }

        // 保存规则
        bzero(&g_context.rules[g_context.rule_count], sizeof(rinetd_rule_t));
        strncpy(g_context.rules[g_context.rule_count].src_addr, src_addr, RINETD_MAX_ADDR_LEN - 1);
        g_context.rules[g_context.rule_count].src_port = (uint16_t)src_port;
        strncpy(g_context.rules[g_context.rule_count].dst_addr, dst_addr, RINETD_MAX_ADDR_LEN - 1);
        g_context.rules[g_context.rule_count].dst_port = (uint16_t)dst_port;

        INFRA_LOG_INFO("Loaded rule %d: %s:%d -> %s:%d", 
            g_context.rule_count,
            g_context.rules[g_context.rule_count].src_addr,
            g_context.rules[g_context.rule_count].src_port,
            g_context.rules[g_context.rule_count].dst_addr,
            g_context.rules[g_context.rule_count].dst_port);

        g_context.rule_count++;
    }

    fclose(fp);

    if (g_context.rule_count == 0) {
        INFRA_LOG_ERROR("No valid rules found in config file");
        return INFRA_ERROR_INVALID_CONFIG;
    }

    INFRA_LOG_INFO("Loaded %d rules from %s", g_context.rule_count, path);
    return INFRA_OK;
}

infra_error_t rinetd_save_config(const char* path) {
    if (!path) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 尝试打开配置文件
    FILE* fp = fopen(path, "w");
    if (fp == NULL) {
        INFRA_LOG_ERROR("Failed to open config file: %s", path);
        return INFRA_ERROR_IO;
    }

    // 保存规则
    for (int i = 0; i < g_context.rule_count; i++) {
        fprintf(fp, "%s %d %s %d\n",
            g_context.rules[i].src_addr,
            g_context.rules[i].src_port,
            g_context.rules[i].dst_addr,
            g_context.rules[i].dst_port);
    }

    fclose(fp);

    INFRA_LOG_INFO("Saved %d rules to %s", g_context.rule_count, path);
    return INFRA_OK;
} 
