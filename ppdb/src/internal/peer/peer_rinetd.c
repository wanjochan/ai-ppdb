#include "internal/peer/peer_rinetd.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_mux.h"

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

static rinetd_context_t g_context = {0};

//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------

static void* forward_thread(void* arg);
static void* backward_thread(void* arg);
static void* listener_thread(void* arg);
static infra_error_t create_listener(rinetd_rule_t* rule);
static infra_error_t stop_listener(int rule_index);
static rinetd_session_t* create_forward_session(infra_socket_t client_sock, rinetd_rule_t* rule);
static void cleanup_forward_session(rinetd_session_t* session);
static infra_error_t init_session_list(void);
static void cleanup_session_list(void);
static infra_error_t add_session_to_list(rinetd_session_t* session);
static void remove_session_from_list(rinetd_session_t* session);
static infra_error_t start_service(const char* program_path);
static infra_error_t stop_service(void);
static infra_error_t show_status(void);
static infra_error_t parse_config_file(const char* path);
static infra_error_t start_listeners(void);

//-----------------------------------------------------------------------------
// Command Line Options
//-----------------------------------------------------------------------------

const poly_cmd_option_t rinetd_options[] = {
    {"config", "Config file path", true},
    {"start", "Start the service", false},
    {"stop", "Stop the service", false},
    {"status", "Show rinetd service status", false},
};

const int rinetd_option_count = sizeof(rinetd_options) / sizeof(rinetd_options[0]);

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

static void* forward_thread(void* arg) {
    rinetd_session_t* session = (rinetd_session_t*)arg;
    infra_socket_t src = session->client_sock;
    infra_socket_t dst = session->server_sock;
    char buffer[RINETD_BUFFER_SIZE];
    size_t bytes_received = 0;
    size_t bytes_sent = 0;

    while (session->active) {
        // Forward data from client to server
        if (src) {
            infra_error_t err = infra_net_recv(src, buffer, sizeof(buffer), &bytes_received);
            if (err != INFRA_OK || bytes_received == 0) {
                break;
            }

            if (dst) {
                err = infra_net_send(dst, buffer, bytes_received, &bytes_sent);
                if (err != INFRA_OK || bytes_sent != bytes_received) {
                    break;
                }
            }
        }
    }

    session->active = false;  // Signal other thread to stop
    return NULL;
}

static void* backward_thread(void* arg) {
    rinetd_session_t* session = (rinetd_session_t*)arg;
    infra_socket_t src = session->server_sock;
    infra_socket_t dst = session->client_sock;
    char buffer[RINETD_BUFFER_SIZE];
    size_t bytes_received = 0;
    size_t bytes_sent = 0;

    while (session->active) {
        // Forward data from server to client
        if (src) {
            infra_error_t err = infra_net_recv(src, buffer, sizeof(buffer), &bytes_received);
            if (err != INFRA_OK || bytes_received == 0) {
                break;
            }

            if (dst) {
                err = infra_net_send(dst, buffer, bytes_received, &bytes_sent);
                if (err != INFRA_OK || bytes_sent != bytes_received) {
                    break;
                }
            }
        }
    }

    session->active = false;  // Signal other thread to stop
    return NULL;
}

// 添加线程参数结构
typedef struct {
    rinetd_rule_t* rule;
    infra_socket_t listener;
} listener_thread_param_t;

static void* listener_thread(void* arg) {
    listener_thread_param_t* param = (listener_thread_param_t*)arg;
    rinetd_rule_t* rule = param->rule;
    infra_socket_t listener = param->listener;
    
    // Accept loop
    while (g_context.running) {
        infra_socket_t client = NULL;
        infra_net_addr_t client_addr = {0};
        infra_error_t err = infra_net_accept(listener, &client, &client_addr);
        if (err != INFRA_OK) {
            continue;
        }

        // Create forward session
        rinetd_session_t* session = create_forward_session(client, rule);
        if (!session) {
            infra_net_close(client);
            continue;
        }
    }

    // 清理
    infra_net_close(listener);
    free(param);
    return NULL;
}

static infra_error_t create_listener(rinetd_rule_t* rule) {
    if (!rule) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 创建监听socket
    infra_socket_t listener = NULL;
    infra_config_t config = {0};
    infra_error_t err = infra_net_create(&listener, false, &config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create listener socket");
        return err;
    }

    // 设置地址重用
    err = infra_net_set_reuseaddr(listener, true);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set socket options");
        infra_net_close(listener);
        return err;
    }

    // 绑定地址
    infra_net_addr_t addr = {0};
    addr.host = rule->src_addr;
    addr.port = rule->src_port;
    err = infra_net_bind(listener, &addr);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to bind listener socket");
        infra_net_close(listener);
        return err;
    }

    // 开始监听
    err = infra_net_listen(listener);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to start listening");
        infra_net_close(listener);
        return err;
    }

    // 创建线程参数
    listener_thread_param_t* param = malloc(sizeof(listener_thread_param_t));
    if (!param) {
        INFRA_LOG_ERROR("Failed to allocate thread parameter");
        infra_net_close(listener);
        return INFRA_ERROR_NO_MEMORY;
    }
    param->rule = rule;
    param->listener = listener;

    // 创建监听线程
    for (int i = 0; i < RINETD_MAX_RULES; i++) {
        if (!g_context.listener_threads[i]) {
            err = infra_thread_create(&g_context.listener_threads[i], listener_thread, param);
            if (err != INFRA_OK) {
                free(param);
                infra_net_close(listener);
                return err;
            }
            break;
        }
    }

    return INFRA_OK;
}

static infra_error_t stop_listener(int rule_index) {
    infra_thread_t* thread = NULL;

    infra_mutex_lock(g_context.mutex);
    thread = g_context.listener_threads[rule_index];
    g_context.listener_threads[rule_index] = NULL;
    infra_mutex_unlock(g_context.mutex);

    if (thread != NULL) {
        infra_thread_join(thread);
    }
    return INFRA_OK;
}

static rinetd_session_t* create_forward_session(infra_socket_t client_sock, rinetd_rule_t* rule) {
    if (!client_sock || !rule) {
        return NULL;
    }

    // Create server socket
    infra_socket_t server_sock = NULL;
    infra_config_t config = {0};
    infra_error_t err = infra_net_create(&server_sock, false, &config);
    if (err != INFRA_OK) {
        return NULL;
    }

    // Connect to destination
    infra_net_addr_t addr = {0};
    addr.host = rule->dst_addr;
    addr.port = rule->dst_port;
    err = infra_net_connect(&addr, &server_sock, &config);
    if (err != INFRA_OK) {
        infra_net_close(server_sock);
        return NULL;
    }

    // Find free session slot
    rinetd_session_t* session = NULL;
    infra_mutex_lock(g_context.mutex);
    for (int i = 0; i < RINETD_MAX_RULES; i++) {
        if (!g_context.active_sessions[i].active) {
            session = &g_context.active_sessions[i];
            break;
        }
    }

    if (!session) {
        infra_mutex_unlock(g_context.mutex);
        infra_net_close(server_sock);
        return NULL;
    }

    // Initialize session
    memset(session, 0, sizeof(rinetd_session_t));
    session->client_sock = client_sock;
    session->server_sock = server_sock;
    session->rule = rule;
    session->active = true;

    // Create forward thread (client to server)
    err = infra_thread_create(&session->forward_thread, forward_thread, session);
    if (err != INFRA_OK) {
        session->active = false;
        infra_mutex_unlock(g_context.mutex);
        infra_net_close(server_sock);
        return NULL;
    }

    // Create backward thread (server to client)
    err = infra_thread_create(&session->backward_thread, backward_thread, session);
    if (err != INFRA_OK) {
        session->active = false;
        infra_thread_join(session->forward_thread);
        infra_mutex_unlock(g_context.mutex);
        infra_net_close(server_sock);
        return NULL;
    }

    g_context.session_count++;
    infra_mutex_unlock(g_context.mutex);
    return session;
}

static void cleanup_forward_session(rinetd_session_t* session) {
    if (session == NULL || g_context.mutex == NULL) {
        return;
    }

    // 先停止会话
    infra_mutex_lock(g_context.mutex);
    session->active = false;
    infra_mutex_unlock(g_context.mutex);

    // 等待线程结束
    if (session->forward_thread != NULL) {
        infra_thread_join(session->forward_thread);
        session->forward_thread = NULL;
    }

    if (session->backward_thread != NULL) {
        infra_thread_join(session->backward_thread);
        session->backward_thread = NULL;
    }

    // 关闭socket
    infra_mutex_lock(g_context.mutex);
    if (session->client_sock != NULL) {
        infra_net_close(session->client_sock);
        session->client_sock = NULL;
    }
    
    if (session->server_sock != NULL) {
        infra_net_close(session->server_sock);
        session->server_sock = NULL;
    }

    // 从列表中移除
    remove_session_from_list(session);
    infra_mutex_unlock(g_context.mutex);
}

static infra_error_t init_session_list(void) {
    // We don't need to allocate dynamically anymore since we're using a fixed array
    memset(g_context.active_sessions, 0, 
        RINETD_MAX_RULES * sizeof(rinetd_session_t));
    g_context.session_count = 0;
    return INFRA_OK;
}

static void cleanup_session_list(void) {
    // Cleanup all active sessions
    infra_mutex_lock(g_context.mutex);
    int count = g_context.session_count;
    infra_mutex_unlock(g_context.mutex);

    while (count > 0) {
        infra_mutex_lock(g_context.mutex);
        if (g_context.session_count > 0) {
            rinetd_session_t* session = &g_context.active_sessions[0];
            infra_mutex_unlock(g_context.mutex);
            cleanup_forward_session(session);
        } else {
            infra_mutex_unlock(g_context.mutex);
            break;
        }
        
        infra_mutex_lock(g_context.mutex);
        count = g_context.session_count;
        infra_mutex_unlock(g_context.mutex);
    }
    
    // Clear all sessions
    memset(g_context.active_sessions, 0, 
        RINETD_MAX_RULES * sizeof(rinetd_session_t));
    g_context.session_count = 0;
}

static infra_error_t add_session_to_list(rinetd_session_t* session) {
    infra_mutex_lock(g_context.mutex);
    if (g_context.session_count >= RINETD_MAX_RULES) {
        infra_mutex_unlock(g_context.mutex);
        return INFRA_ERROR_NO_MEMORY;
    }

    memcpy(&g_context.active_sessions[g_context.session_count], 
        session, sizeof(rinetd_session_t));
    g_context.session_count++;
    infra_mutex_unlock(g_context.mutex);
    return INFRA_OK;
}

static void remove_session_from_list(rinetd_session_t* session) {
    infra_mutex_lock(g_context.mutex);
    for (int i = 0; i < g_context.session_count; i++) {
        // Compare session by sockets instead of pointer
        if (g_context.active_sessions[i].client_sock == session->client_sock &&
            g_context.active_sessions[i].server_sock == session->server_sock) {
            // Move last session to this slot if not the last one
            if (i < g_context.session_count - 1) {
                memcpy(&g_context.active_sessions[i],
                    &g_context.active_sessions[g_context.session_count - 1],
                    sizeof(rinetd_session_t));
            }
            // Clear the last slot
            memset(&g_context.active_sessions[g_context.session_count - 1], 
                0, sizeof(rinetd_session_t));
            g_context.session_count--;
            break;
        }
    }
    infra_mutex_unlock(g_context.mutex);
}

//-----------------------------------------------------------------------------
// Core Functions Implementation
//-----------------------------------------------------------------------------

infra_error_t rinetd_init(const infra_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (g_context.mutex != NULL) {
        return INFRA_ERROR_ALREADY_EXISTS;
    }

    // 清空上下文
    memset(&g_context, 0, sizeof(g_context));

    // 创建互斥锁
    infra_error_t err = infra_mutex_create(&g_context.mutex);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create mutex: %d", err);
        return err;
    }

    // 创建多路复用器
    err = infra_mux_create(config, &g_context.mux);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create multiplexer: %d", err);
        infra_mutex_destroy(g_context.mutex);
        g_context.mutex = NULL;
        return err;
    }

    // 初始化会话列表
    err = init_session_list();
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize session list: %d", err);
        infra_mux_destroy(g_context.mux);
        g_context.mux = NULL;
        infra_mutex_destroy(g_context.mutex);
        g_context.mutex = NULL;
        return err;
    }

    // 尝试加载默认配置文件
    if (g_context.config_path[0] == '\0') {
        strncpy(g_context.config_path, "ppdb/rinetd.conf", RINETD_MAX_PATH_LEN - 1);
        g_context.config_path[RINETD_MAX_PATH_LEN - 1] = '\0';
        
        err = rinetd_load_config(g_context.config_path);
        if (err != INFRA_OK) {
            INFRA_LOG_WARN("Failed to load default config file: %s", g_context.config_path);
        }
    }

    INFRA_LOG_INFO("Rinetd service initialized successfully");
    return INFRA_OK;
}

infra_error_t rinetd_cleanup(void) {
    if (g_context.running) {
        INFRA_LOG_ERROR("Service is still running");
        return INFRA_ERROR_BUSY;
    }

    cleanup_session_list();

    if (g_context.mux != NULL) {
        infra_mux_destroy(g_context.mux);
        g_context.mux = NULL;
    }

    if (g_context.mutex != NULL) {
        infra_mutex_destroy(g_context.mutex);
        g_context.mutex = NULL;
    }

    memset(&g_context, 0, sizeof(g_context));
    return INFRA_OK;
}

static infra_error_t parse_config_file(const char* path) {
    if (path == NULL) {
        INFRA_LOG_ERROR("Invalid config file path");
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (g_context.mutex == NULL) {
        INFRA_LOG_ERROR("Service is not initialized");
        return INFRA_ERROR_NOT_SUPPORTED;
    }

    infra_mutex_lock(g_context.mutex);
    if (g_context.running) {
        infra_mutex_unlock(g_context.mutex);
        INFRA_LOG_ERROR("Cannot change configuration while service is running");
        return INFRA_ERROR_BUSY;
    }
    infra_mutex_unlock(g_context.mutex);

    // 修正配置文件路径
    char config_path[RINETD_MAX_PATH_LEN];
    if (path[0] == '/' || path[0] == '\\' || (path[1] == ':' && (path[2] == '\\' || path[2] == '/'))) {
        // 绝对路径
        strncpy(config_path, path, RINETD_MAX_PATH_LEN - 1);
    } else {
        // 相对路径
        char cwd[RINETD_MAX_PATH_LEN];
        infra_error_t err = infra_get_cwd(cwd, sizeof(cwd));
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to get current working directory");
            return err;
        }
        snprintf(config_path, sizeof(config_path), "%s/%s", cwd, path);
    }
    config_path[RINETD_MAX_PATH_LEN - 1] = '\0';

    // 尝试加载配置文件
    infra_error_t err = rinetd_load_config(config_path);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to load config file: %s", config_path);
        return err;
    }

    return INFRA_OK;  // 总是返回成功，让服务可以继续启动
}

infra_error_t rinetd_load_config(const char* path) {
    if (!path) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (!g_context.mutex) {
        return INFRA_ERROR_NOT_SUPPORTED;
    }

    strncpy(g_context.config_path, path, RINETD_MAX_PATH_LEN - 1);
    g_context.config_path[RINETD_MAX_PATH_LEN - 1] = '\0';

    // Open config file
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        INFRA_LOG_ERROR("Failed to open config file: %s", path);
        return INFRA_ERROR_IO;
    }

    // Clear existing rules
    g_context.rule_count = 0;
    memset(g_context.rules, 0, sizeof(g_context.rules));

    // Parse config file
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        // Parse rule
        rinetd_rule_t rule = {0};
        char src_addr[RINETD_MAX_ADDR_LEN] = {0};
        char dst_addr[RINETD_MAX_ADDR_LEN] = {0};
        int src_port = 0;
        int dst_port = 0;

        if (sscanf(line, "%s %d %s %d", src_addr, &src_port, dst_addr, &dst_port) == 4) {
            strncpy(rule.src_addr, src_addr, RINETD_MAX_ADDR_LEN - 1);
            rule.src_port = src_port;
            strncpy(rule.dst_addr, dst_addr, RINETD_MAX_ADDR_LEN - 1);
            rule.dst_port = dst_port;
            rule.enabled = true;

            // Add rule
            if (g_context.rule_count < RINETD_MAX_RULES) {
                memcpy(&g_context.rules[g_context.rule_count], &rule, sizeof(rule));
                g_context.rule_count++;
                INFRA_LOG_INFO("Added rule: %s:%d -> %s:%d", 
                    rule.src_addr, rule.src_port, rule.dst_addr, rule.dst_port);
            } else {
                INFRA_LOG_ERROR("Too many rules, skipping: %s:%d -> %s:%d",
                    rule.src_addr, rule.src_port, rule.dst_addr, rule.dst_port);
            }
        }
    }

    fclose(fp);
    return INFRA_OK;
}

infra_error_t rinetd_save_config(const char* path) {
    if (path == NULL) {
        INFRA_LOG_ERROR("Invalid config file path");
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Open config file
    FILE* fp = fopen(path, "w");
    if (fp == NULL) {
        INFRA_LOG_ERROR("Failed to open config file: %s", path);
        return INFRA_ERROR_IO;
    }

    // Write header
    fprintf(fp, "# rinetd configuration file\n");
    fprintf(fp, "# format: src_addr src_port dst_addr dst_port\n\n");

    // Write rules
    for (int i = 0; i < g_context.rule_count; i++) {
        if (g_context.rules[i].enabled) {
            fprintf(fp, "%s %d %s %d\n",
                g_context.rules[i].src_addr,
                g_context.rules[i].src_port,
                g_context.rules[i].dst_addr,
                g_context.rules[i].dst_port);
        }
    }

    fclose(fp);
    return INFRA_OK;
}

infra_error_t rinetd_add_rule(const rinetd_rule_t* rule) {
    if (rule == NULL) {
        INFRA_LOG_ERROR("Invalid rule pointer");
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (g_context.rule_count >= RINETD_MAX_RULES) {
        INFRA_LOG_ERROR("Too many rules");
        return INFRA_ERROR_NO_MEMORY;
    }

    // Check for duplicate rules
    for (int i = 0; i < g_context.rule_count; i++) {
        if (g_context.rules[i].enabled &&
            g_context.rules[i].src_port == rule->src_port &&
            strcmp(g_context.rules[i].src_addr, rule->src_addr) == 0) {
            INFRA_LOG_ERROR("Rule already exists for %s:%d",
                rule->src_addr, rule->src_port);
            return INFRA_ERROR_EXISTS;
        }
    }

    // Add new rule
    memcpy(&g_context.rules[g_context.rule_count], rule, sizeof(rinetd_rule_t));
    g_context.rule_count++;

    return INFRA_OK;
}

infra_error_t rinetd_remove_rule(int index) {
    if (index < 0 || index >= g_context.rule_count) {
        INFRA_LOG_ERROR("Invalid rule index");
        return INFRA_ERROR_INVALID_PARAM;
    }

    if (g_context.running) {
        INFRA_LOG_ERROR("Cannot remove rule while service is running");
        return INFRA_ERROR_BUSY;
    }

    // Disable the rule
    g_context.rules[index].enabled = false;

    // Compact rules array if this was the last rule
    if (index == g_context.rule_count - 1) {
        g_context.rule_count--;
    }

    return INFRA_OK;
}

bool rinetd_is_running(void) {
    return g_context.running;
}

//-----------------------------------------------------------------------------
// Command Handlers
//-----------------------------------------------------------------------------

static infra_error_t start_service(const char* program_path) {
    if (!g_context.mutex) {
        INFRA_LOG_ERROR("Service is not initialized");
        return INFRA_ERROR_NOT_SUPPORTED;
    }

    infra_mutex_lock(g_context.mutex);
    
    if (g_context.running) {
        infra_mutex_unlock(g_context.mutex);
        INFRA_LOG_ERROR("Service is already running");
        return INFRA_ERROR_BUSY;
    }

    // 检查是否有可用的规则
    bool has_enabled_rules = false;
    for (int i = 0; i < g_context.rule_count; i++) {
        if (g_context.rules[i].enabled) {
            has_enabled_rules = true;
            break;
        }
    }

    if (!has_enabled_rules) {
        infra_mutex_unlock(g_context.mutex);
        INFRA_LOG_ERROR("No enabled forwarding rules");
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 检查所有端口是否可用
    for (int i = 0; i < g_context.rule_count; i++) {
        if (!g_context.rules[i].enabled) {
            continue;
        }

        infra_socket_t test_sock = NULL;
        infra_config_t config = {0};
        infra_error_t err = infra_net_create(&test_sock, false, &config);
        if (err != INFRA_OK) {
            infra_mutex_unlock(g_context.mutex);
            INFRA_LOG_ERROR("Failed to create test socket");
            return err;
        }

        infra_net_addr_t addr = {0};
        addr.host = g_context.rules[i].src_addr;
        addr.port = g_context.rules[i].src_port;

        err = infra_net_bind(test_sock, &addr);
        infra_net_close(test_sock);

        if (err != INFRA_OK) {
            infra_mutex_unlock(g_context.mutex);
            INFRA_LOG_ERROR("Port %d is already in use", g_context.rules[i].src_port);
            return INFRA_ERROR_BUSY;
        }
    }
    
    // 所有端口都可用，开始真正的服务
    infra_error_t err = start_listeners();
    if (err == INFRA_OK) {
        g_context.running = true;
        INFRA_LOG_INFO("Rinetd service started successfully");
        infra_mutex_unlock(g_context.mutex);
        
        // 保持主线程运行，直到收到停止信号
        while (1) {
            infra_mutex_lock(g_context.mutex);
            bool is_running = g_context.running;
            infra_mutex_unlock(g_context.mutex);
            
            if (!is_running) {
                break;
            }
            
            infra_sleep(1000);  // 每秒检查一次运行状态
        }
    } else {
        INFRA_LOG_ERROR("Failed to start listeners: %d", err);
        infra_mutex_unlock(g_context.mutex);
        return err;
    }
    
    return INFRA_OK;
}

static infra_error_t stop_service(void) {
    if (!g_context.mutex) {
        return INFRA_ERROR_NOT_SUPPORTED;
    }

    if (!g_context.running) {
        return INFRA_ERROR_NOT_SUPPORTED;
    }

    // Stop accepting new connections
    g_context.running = false;

    // Stop all listener threads
    for (int i = 0; i < RINETD_MAX_RULES; i++) {
        if (g_context.listener_threads[i]) {
            infra_thread_join(g_context.listener_threads[i]);
            g_context.listener_threads[i] = NULL;
        }
    }

    // Stop all active sessions
    for (int i = 0; i < RINETD_MAX_RULES; i++) {
        if (g_context.active_sessions[i].active) {
            g_context.active_sessions[i].active = false;
            if (g_context.active_sessions[i].forward_thread) {
                infra_thread_join(g_context.active_sessions[i].forward_thread);
                g_context.active_sessions[i].forward_thread = NULL;
            }
            if (g_context.active_sessions[i].backward_thread) {
                infra_thread_join(g_context.active_sessions[i].backward_thread);
                g_context.active_sessions[i].backward_thread = NULL;
            }
            if (g_context.active_sessions[i].client_sock) {
                infra_net_close(g_context.active_sessions[i].client_sock);
                g_context.active_sessions[i].client_sock = NULL;
            }
            if (g_context.active_sessions[i].server_sock) {
                infra_net_close(g_context.active_sessions[i].server_sock);
                g_context.active_sessions[i].server_sock = NULL;
            }
        }
    }

    g_context.session_count = 0;

    return INFRA_OK;
}

static infra_error_t show_status(void) {
    infra_printf("Checking rinetd service status...\n");
    
    if (g_context.mutex == NULL) {
        infra_printf("Service is not initialized\n");
        return INFRA_OK;
    }

    infra_mutex_lock(g_context.mutex);
    
    if (g_context.running) {
        infra_printf("Service is running\n");
        if (g_context.rule_count > 0) {
            infra_printf("Active forwarding rules:\n");
            for (int i = 0; i < g_context.rule_count; i++) {
                if (g_context.rules[i].enabled) {
                    infra_printf("  %s:%d -> %s:%d\n",
                        g_context.rules[i].src_addr,
                        g_context.rules[i].src_port,
                        g_context.rules[i].dst_addr,
                        g_context.rules[i].dst_port);
                }
            }
            infra_printf("Active sessions: %d\n", g_context.session_count);
        } else {
            infra_printf("No active forwarding rules\n");
        }
    } else {
        infra_printf("Service is not running\n");
        if (g_context.rule_count > 0) {
            infra_printf("Configured forwarding rules:\n");
            for (int i = 0; i < g_context.rule_count; i++) {
                if (g_context.rules[i].enabled) {
                    infra_printf("  %s:%d -> %s:%d\n",
                        g_context.rules[i].src_addr,
                        g_context.rules[i].src_port,
                        g_context.rules[i].dst_addr,
                        g_context.rules[i].dst_port);
                }
            }
        } else {
            infra_printf("No forwarding rules configured\n");
        }
    }

    infra_mutex_unlock(g_context.mutex);
    return INFRA_OK;
}

infra_error_t rinetd_cmd_handler(int argc, char** argv) {
    if (argc < 2) {
        INFRA_LOG_ERROR("Invalid arguments");
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Initialize service if not already initialized
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    infra_error_t err = rinetd_init(&config);
    if (err != INFRA_OK && err != INFRA_ERROR_ALREADY_EXISTS) {
        INFRA_LOG_ERROR("Failed to initialize rinetd service");
        return err;
    }

    // 处理命令行参数
    bool should_start = false;
    const char* config_path = NULL;

    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (strcmp(arg, "--start") == 0) {
            should_start = true;
        } else if (strcmp(arg, "--stop") == 0) {
            err = stop_service();
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to stop service: %d", err);
            }
            return err;
        } else if (strcmp(arg, "--status") == 0) {
            return show_status();
        } else if (strcmp(arg, "--config") == 0) {
            if (i + 1 >= argc) {
                INFRA_LOG_ERROR("Missing config file path");
                return INFRA_ERROR_INVALID_PARAM;
            }
            config_path = argv[++i];
        }
    }

    // 如果指定了配置文件，先加载配置
    if (config_path != NULL) {
        err = parse_config_file(config_path);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to parse config file: %d", err);
            return err;
        }
    }

    // 如果需要启动服务
    if (should_start) {
        err = start_service(argv[0]);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to start service: %d", err);
        }
        return err;
    }

    return INFRA_OK;
}

static infra_error_t start_listeners(void) {
    infra_error_t err = INFRA_OK;
    
    // 为每个启用的规则创建监听器
    for (int i = 0; i < g_context.rule_count; i++) {
        if (g_context.rules[i].enabled) {
            err = create_listener(&g_context.rules[i]);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to create listener for rule %d: %d", i, err);
                // 停止已创建的监听器
                for (int j = 0; j < i; j++) {
                    stop_listener(j);
                }
                return err;
            }
        }
    }
    
    return INFRA_OK;
} 