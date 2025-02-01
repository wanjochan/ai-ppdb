//rinet: ports forwarding

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_net.h"
#include "internal/peer/peer_service.h"
#include "internal/poly/poly_poll.h"

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
// Command Line Options
//-----------------------------------------------------------------------------

static const poly_cmd_option_t rinetd_options[] = {
    {"config", "Config file path", true},
    {"start", "Start the service", false},
    {"stop", "Stop the service", false},
    {"status", "Show service status", false},
};

static const int rinetd_option_count = sizeof(rinetd_options) / sizeof(rinetd_options[0]);

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
    rinetd_rule_t rules[RINETD_MAX_RULES];  // 规则数组
    int rule_count;                      // 当前规则数量
    poly_poll_context_t* poll_ctx;       // poll上下文
} rinetd_context_t;

//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------

static infra_error_t rinetd_init(const infra_config_t* config);
static infra_error_t rinetd_cleanup(void);
static infra_error_t rinetd_start(void);
static infra_error_t rinetd_stop(void);
static bool rinetd_is_running(void);
static infra_error_t rinetd_cmd_handler(int argc, char** argv);
static infra_error_t rinetd_load_config(const char* path);
static infra_error_t rinetd_save_config(const char* path);

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

// 服务实例
static infra_config_t g_rinetd_default_config;
static peer_service_config_t g_rinetd_service_config;
peer_service_t g_rinetd_service;
static rinetd_context_t g_context;

// 初始化函数
static void __attribute__((constructor)) rinetd_init_globals(void) {
    // 初始化默认配置
    g_rinetd_default_config = (infra_config_t){
        .memory = {
            .use_memory_pool = false,
            .pool_initial_size = 1024 * 1024,  // 1MB
            .pool_alignment = sizeof(void*)
        },
        .log = {
            .level = INFRA_LOG_LEVEL_INFO,
            .log_file = NULL
        },
        .net = {
            .flags = 0,  // 默认使用阻塞模式
            .connect_timeout_ms = 1000,  // 1秒连接超时
            .read_timeout_ms = 0,        // 无读取超时
            .write_timeout_ms = 0        // 无写入超时
        }
    };

    // 初始化服务配置
    g_rinetd_service_config = (peer_service_config_t){
        .name = "rinetd",
        .type = SERVICE_TYPE_RINETD,
        .options = rinetd_options,
        .option_count = rinetd_option_count,
        .config = &g_rinetd_default_config,
        .config_path = NULL
    };

    // 初始化服务实例
    g_rinetd_service = (peer_service_t){
        .config = g_rinetd_service_config,
        .state = SERVICE_STATE_UNKNOWN,
        .init = rinetd_init,
        .cleanup = rinetd_cleanup,
        .start = rinetd_start,
        .stop = rinetd_stop,
        .is_running = rinetd_is_running,
        .cmd_handler = rinetd_cmd_handler
    };

    // 初始化上下文
    memset(&g_context, 0, sizeof(g_context));
}

extern void bzero(void* s, size_t n);

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
static void handle_connection(void* user_data) {
    poly_poll_handler_args_t* args = (poly_poll_handler_args_t*)user_data;
    if (!args || !args->client || !args->user_data) {
        INFRA_LOG_ERROR("Invalid connection parameters");
        if (args && args->client) infra_net_close(args->client);
        if (args) free(args);
        return;
    }

    // 获取规则索引
    int rule_idx = (int)(intptr_t)args->user_data;
    if (rule_idx < 0 || rule_idx >= g_context.rule_count) {
        INFRA_LOG_ERROR("Invalid rule index: %d", rule_idx);
        infra_net_close(args->client);
        free(args);
        return;
    }

    rinetd_rule_t* rule = &g_context.rules[rule_idx];

    // 连接目标服务器
    infra_socket_t server = NULL;
    infra_config_t config = {0};
    infra_error_t err = infra_net_create(&server, false, &config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create server socket: %d", err);
        infra_net_close(args->client);
        free(args);
        return;
    }

    // 连接目标地址
    infra_net_addr_t addr = {
        .host = rule->dst_addr,
        .port = rule->dst_port
    };
    err = infra_net_connect(&addr, &server, &config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to connect to server %s:%d: %d", 
            rule->dst_addr, rule->dst_port, err);
        infra_net_close(args->client);
        infra_net_close(server);
        free(args);
        return;
    }

    INFRA_LOG_INFO("Connected to server %s:%d", rule->dst_addr, rule->dst_port);

    // 设置非阻塞模式
    err = infra_net_set_nonblock(args->client, true);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set client nonblock: %d", err);
        infra_net_close(args->client);
        infra_net_close(server);
        free(args);
        return;
    }

    err = infra_net_set_nonblock(server, true);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set server nonblock: %d", err);
        infra_net_close(args->client);
        infra_net_close(server);
        free(args);
        return;
    }

    // 创建会话
    rinetd_session_t* session = malloc(sizeof(rinetd_session_t));
    if (!session) {
        INFRA_LOG_ERROR("Failed to allocate session");
        infra_net_close(args->client);
        infra_net_close(server);
        free(args);
        return;
    }

    session->client = args->client;
    session->server = server;
    session->c2s_failed = false;
    session->s2c_failed = false;

    // 不再需要 args 了
    free(args);

    // 设置 poll
    struct pollfd fds[2] = {0};
    fds[0].fd = infra_net_get_fd(session->client);
    fds[0].events = POLLIN;
    fds[1].fd = infra_net_get_fd(session->server);
    fds[1].events = POLLIN;

    // 开始转发数据
    char buffer[RINETD_BUFFER_SIZE];
    while (!session->c2s_failed && !session->s2c_failed) {
        // 等待事件，1秒超时
        int ret = poll(fds, 2, 1000);
        if (ret < 0) {
            if (errno == EINTR) continue;
            INFRA_LOG_ERROR("Poll failed: %d", errno);
            break;
        }
        if (ret == 0) continue;  // 超时，继续

        // 客户端到服务器
        if (!session->c2s_failed && (fds[0].revents & POLLIN)) {
            err = forward_data(session->client, session->server, buffer, "C->S");
            if (err != INFRA_OK) {
                if (err != INFRA_ERROR_WOULD_BLOCK && err != INFRA_ERROR_TIMEOUT) {
                    session->c2s_failed = true;
                    INFRA_LOG_DEBUG("Client to server connection failed: %d", err);
                }
            }
        }

        // 服务器到客户端
        if (!session->s2c_failed && (fds[1].revents & POLLIN)) {
            err = forward_data(session->server, session->client, buffer, "S->C");
            if (err != INFRA_OK) {
                if (err != INFRA_ERROR_WOULD_BLOCK && err != INFRA_ERROR_TIMEOUT) {
                    session->s2c_failed = true;
                    INFRA_LOG_DEBUG("Server to client connection failed: %d", err);
                }
            }
        }

        // 检查连接错误
        if ((fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) ||
            (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL))) {
            INFRA_LOG_DEBUG("Connection error detected");
            break;
        }
    }

    // 清理资源
    INFRA_LOG_DEBUG("Connection closed");
    infra_net_close(session->client);
    infra_net_close(session->server);
    free(session);
}

// 启动服务
static infra_error_t rinetd_start(void) {
    if (g_context.running) {
        INFRA_LOG_ERROR("Service already running");
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    // 检查服务状态
    if (g_rinetd_service.state == SERVICE_STATE_UNKNOWN) {
        INFRA_LOG_ERROR("Service is not initialized");
        return INFRA_ERROR_INVALID_STATE;
    }

    if (g_rinetd_service.state != SERVICE_STATE_STOPPED) {
        INFRA_LOG_ERROR("Service is in invalid state: %d", g_rinetd_service.state);
        return INFRA_ERROR_INVALID_STATE;
    }

    // 检查是否有规则
    if (g_context.rule_count == 0) {
        INFRA_LOG_ERROR("No rules configured");
        return INFRA_ERROR_INVALID_STATE;
    }

    // 更新服务状态
    g_rinetd_service.state = SERVICE_STATE_STARTING;

    // 创建 poly_poll 上下文
    g_context.poll_ctx = malloc(sizeof(poly_poll_context_t));
    if (!g_context.poll_ctx) {
        INFRA_LOG_ERROR("Failed to allocate poll context");
        g_rinetd_service.state = SERVICE_STATE_STOPPED;
        return INFRA_ERROR_NO_MEMORY;
    }

    // 初始化 poly_poll
    poly_poll_config_t config = {
        .min_threads = RINETD_MIN_THREADS,
        .max_threads = RINETD_MAX_THREADS,
        .queue_size = RINETD_MAX_THREADS * 2,
        .max_listeners = RINETD_MAX_RULES
    };
    
    infra_error_t err = poly_poll_init(g_context.poll_ctx, &config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to init poll context: %d", err);
        free(g_context.poll_ctx);
        g_context.poll_ctx = NULL;
        g_rinetd_service.state = SERVICE_STATE_STOPPED;
        return err;
    }
    
    // 添加监听器
    for (int i = 0; i < g_context.rule_count; i++) {
        poly_poll_listener_t listener = {0};
        strncpy(listener.bind_addr, g_context.rules[i].src_addr, POLY_MAX_ADDR_LEN - 1);
        listener.bind_addr[POLY_MAX_ADDR_LEN - 1] = '\0';
        listener.bind_port = g_context.rules[i].src_port;
        listener.user_data = (void*)(intptr_t)i;  // 传递规则索引

        err = poly_poll_add_listener(g_context.poll_ctx, &listener);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to add listener %s:%d: %d",
                g_context.rules[i].src_addr, g_context.rules[i].src_port, err);
            goto error;
        }

        INFRA_LOG_INFO("Added listener %s:%d -> %s:%d",
            g_context.rules[i].src_addr, g_context.rules[i].src_port,
            g_context.rules[i].dst_addr, g_context.rules[i].dst_port);
    }
    
    // 设置连接处理器
    poly_poll_set_handler(g_context.poll_ctx, handle_connection);
    
    // 启动服务
    err = poly_poll_start(g_context.poll_ctx);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to start poll: %d", err);
        goto error;
    }

    // 标记服务为运行状态
    g_context.running = true;
    g_rinetd_service.state = SERVICE_STATE_RUNNING;

    INFRA_LOG_INFO("Service started with %d rules", g_context.rule_count);
    return INFRA_OK;

error:
    if (g_context.poll_ctx) {
        poly_poll_cleanup(g_context.poll_ctx);
        free(g_context.poll_ctx);
        g_context.poll_ctx = NULL;
    }
    g_context.running = false;
    g_rinetd_service.state = SERVICE_STATE_STOPPED;
    return err;
}

// 停止服务
static infra_error_t rinetd_stop(void) {
    if (!g_context.running) {
        return INFRA_OK;
    }

    // 检查服务状态
    if (g_rinetd_service.state != SERVICE_STATE_RUNNING) {
        INFRA_LOG_ERROR("Service is in invalid state: %d", g_rinetd_service.state);
        return INFRA_ERROR_INVALID_STATE;
    }

    // 更新服务状态
    g_rinetd_service.state = SERVICE_STATE_STOPPING;

    // 停止 poll 上下文
    if (g_context.poll_ctx) {
        infra_error_t err = poly_poll_stop(g_context.poll_ctx);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to stop poll: %d", err);
            // 继续清理，但返回错误
        }

        poly_poll_cleanup(g_context.poll_ctx);  // 忽略返回值，因为是void
        free(g_context.poll_ctx);
        g_context.poll_ctx = NULL;
    }

    // 清理上下文
    g_context.running = false;
    g_context.rule_count = 0;
    memset(g_context.rules, 0, sizeof(g_context.rules));
    memset(g_context.config_path, 0, sizeof(g_context.config_path));

    // 更新服务状态
    g_rinetd_service.state = SERVICE_STATE_STOPPED;
    g_rinetd_service.config.config_path = NULL;

    INFRA_LOG_INFO("Service stopped");
    return INFRA_OK;
}

infra_error_t rinetd_init(const infra_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 检查服务状态
    if (g_rinetd_service.state != SERVICE_STATE_UNKNOWN && 
        g_rinetd_service.state != SERVICE_STATE_STOPPED) {
        INFRA_LOG_ERROR("Service is in invalid state: %d", g_rinetd_service.state);
        return INFRA_ERROR_INVALID_STATE;
    }

    // 清零上下文
    memset(&g_context, 0, sizeof(g_context));

    // 复制配置
    if (g_rinetd_service.config.config) {
        memcpy(g_rinetd_service.config.config, config, sizeof(infra_config_t));
    }

    // 更新服务状态
    g_rinetd_service.state = SERVICE_STATE_STOPPED;
    g_rinetd_service.config.config_path = NULL;

    INFRA_LOG_INFO("Service initialized");
    return INFRA_OK;
}

infra_error_t rinetd_cleanup(void) {
    // 检查服务状态
    if (g_rinetd_service.state == SERVICE_STATE_RUNNING ||
        g_rinetd_service.state == SERVICE_STATE_STARTING) {
        INFRA_LOG_ERROR("Cannot cleanup while service is running");
        return INFRA_ERROR_INVALID_STATE;
    }

    // 如果服务正在停止，等待它完成
    if (g_rinetd_service.state == SERVICE_STATE_STOPPING) {
        INFRA_LOG_ERROR("Service is still stopping");
        return INFRA_ERROR_BUSY;
    }

    // 停止服务（如果还在运行）
    if (g_context.running) {
        rinetd_stop();
    }

    // 清理上下文
    memset(&g_context, 0, sizeof(g_context));
    
    // 更新服务状态
    g_rinetd_service.state = SERVICE_STATE_UNKNOWN;
    g_rinetd_service.config.config_path = NULL;

    INFRA_LOG_INFO("Service cleaned up");
    return INFRA_OK;
}

bool rinetd_is_running(void) {
    return g_context.running;
}

infra_error_t rinetd_cmd_handler(int argc, char** argv) {
    if (argc < 1) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Parse command line
    bool should_start = false;
    bool should_stop = false;
    bool should_show_status = false;
    const char* config_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--start") == 0) {
            should_start = true;
        } else if (strcmp(argv[i], "--stop") == 0) {
            should_stop = true;
        } else if (strcmp(argv[i], "--status") == 0) {
            should_show_status = true;
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

    // 检查命令互斥性
    if ((should_start && should_stop) || 
        (should_start && should_show_status) || 
        (should_stop && should_show_status)) {
        INFRA_LOG_ERROR("Cannot specify multiple operations");
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 处理状态查询
    if (should_show_status) {
        const char* state_str = "unknown";
        switch (g_rinetd_service.state) {
            case SERVICE_STATE_STOPPED:
                state_str = "stopped";
                break;
            case SERVICE_STATE_STARTING:
                state_str = "starting";
                break;
            case SERVICE_STATE_RUNNING:
                state_str = "running";
                break;
            case SERVICE_STATE_STOPPING:
                state_str = "stopping";
                break;
        }
        infra_printf("Service is %s\n", state_str);
        if (g_rinetd_service.state == SERVICE_STATE_RUNNING) {
            infra_printf("Active rules:\n");
            for (int i = 0; i < g_context.rule_count; i++) {
                infra_printf("  %d: %s:%d -> %s:%d\n", i + 1,
                    g_context.rules[i].src_addr,
                    g_context.rules[i].src_port,
                    g_context.rules[i].dst_addr,
                    g_context.rules[i].dst_port);
            }
        }
        return INFRA_OK;
    }

    // 处理停止命令
    if (should_stop) {
        if (g_rinetd_service.state != SERVICE_STATE_RUNNING) {
            INFRA_LOG_ERROR("Service is not running");
            return INFRA_ERROR_INVALID_STATE;
        }
        return rinetd_stop();
    }

    // 处理启动命令
    if (should_start) {
        // 检查服务状态
        if (g_rinetd_service.state == SERVICE_STATE_RUNNING) {
            INFRA_LOG_ERROR("Service is already running");
            return INFRA_ERROR_ALREADY_EXISTS;
        }

        if (g_rinetd_service.state == SERVICE_STATE_STARTING || 
            g_rinetd_service.state == SERVICE_STATE_STOPPING) {
            INFRA_LOG_ERROR("Service is in transition state: %d", g_rinetd_service.state);
            return INFRA_ERROR_BUSY;
        }

        // 检查配置文件
        if (!config_path) {
            INFRA_LOG_ERROR("Config file is required to start the service");
            return INFRA_ERROR_INVALID_PARAM;
        }

        // 初始化服务（如果需要）
        if (g_rinetd_service.state == SERVICE_STATE_UNKNOWN) {
            infra_config_t config = INFRA_DEFAULT_CONFIG;
            infra_error_t err = rinetd_init(&config);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to initialize service: %d", err);
                return err;
            }
        }

        // 加载配置文件
        INFRA_LOG_DEBUG("Loading config file: %s", config_path);
        infra_error_t err = rinetd_load_config(config_path);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to load config file: %d", err);
            return err;
        }

        // 启动服务
        INFRA_LOG_DEBUG("Starting rinetd service");
        err = rinetd_start();
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to start rinetd service: %d", err);
            return err;
        }

        INFRA_LOG_INFO("Rinetd service started successfully");
        return INFRA_OK;
    }

    // 如果没有指定任何操作，显示状态
    const char* state_str = "unknown";
    switch (g_rinetd_service.state) {
        case SERVICE_STATE_STOPPED:
            state_str = "stopped";
            break;
        case SERVICE_STATE_STARTING:
            state_str = "starting";
            break;
        case SERVICE_STATE_RUNNING:
            state_str = "running";
            break;
        case SERVICE_STATE_STOPPING:
            state_str = "stopping";
            break;
    }
    infra_printf("Service is %s\n", state_str);
    if (g_rinetd_service.state == SERVICE_STATE_RUNNING) {
        infra_printf("Active rules:\n");
        for (int i = 0; i < g_context.rule_count; i++) {
            infra_printf("  %d: %s:%d -> %s:%d\n", i + 1,
                g_context.rules[i].src_addr,
                g_context.rules[i].src_port,
                g_context.rules[i].dst_addr,
                g_context.rules[i].dst_port);
        }
    }
    return INFRA_OK;
}

infra_error_t rinetd_load_config(const char* path) {
    if (!path) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 检查服务状态
    if (g_rinetd_service.state == SERVICE_STATE_UNKNOWN) {
        INFRA_LOG_ERROR("Service is not initialized");
        return INFRA_ERROR_INVALID_STATE;
    }

    if (g_rinetd_service.state != SERVICE_STATE_STOPPED) {
        INFRA_LOG_ERROR("Cannot load config while service is in state: %d", g_rinetd_service.state);
        return INFRA_ERROR_INVALID_STATE;
    }

    // 重置规则计数
    g_context.rule_count = 0;

    // 尝试加载配置文件
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        INFRA_LOG_ERROR("Failed to open config file: %s", path);
        return INFRA_ERROR_IO;
    }

    // 保存配置文件路径
    strncpy(g_context.config_path, path, RINETD_MAX_PATH_LEN - 1);
    g_context.config_path[RINETD_MAX_PATH_LEN - 1] = '\0';

    // 解析配置文件
    char line[256];
    int line_num = 0;
    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        
        // 跳过注释和空行
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' || line[0] == '\0') {
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

        // 移除行尾的换行符
        char* newline = strchr(line, '\n');
        if (newline) *newline = '\0';
        newline = strchr(line, '\r');
        if (newline) *newline = '\0';

        // 解析字段
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
            g_context.rule_count + 1,
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

    // 更新服务配置路径
    g_rinetd_service.config.config_path = g_context.config_path;

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
