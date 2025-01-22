#include "internal/peer/peer_rinetd.h"

extern void bzero(void* s, size_t n);

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

static struct {
    bool running;
    rinetd_rule_t rule;
    infra_socket_t listener;
    infra_thread_pool_t* pool;
} g_context = {0};

//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------

static void* handle_connection(void* arg);
static infra_error_t create_listener(void);

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

// 转发数据
static infra_error_t forward_data(infra_socket_t src, infra_socket_t dst, char* buffer, const char* direction) {
    size_t bytes_received = 0;
    size_t bytes_sent = 0;
    size_t total_sent = 0;
    
    infra_error_t err = infra_net_recv(src, buffer, RINETD_BUFFER_SIZE, &bytes_received);
    if (err != INFRA_OK) {
        if (err != INFRA_ERROR_TIMEOUT && err != INFRA_ERROR_WOULD_BLOCK) {
            INFRA_LOG_ERROR("Failed to receive data: %d", err);
            return err;
        }
        return INFRA_ERROR_TIMEOUT;
    }
    
    if (bytes_received == 0) {
        INFRA_LOG_DEBUG("Peer closed connection");
        return INFRA_ERROR_CLOSED;
    }

    // 循环发送直到所有数据都发送完
    while (total_sent < bytes_received) {
        err = infra_net_send(dst, buffer + total_sent, bytes_received - total_sent, &bytes_sent);
        if (err != INFRA_OK) {
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                // 发送缓冲区满，等待下一次循环
                return INFRA_ERROR_WOULD_BLOCK;
            }
            if (err != INFRA_ERROR_TIMEOUT) {
                INFRA_LOG_ERROR("Failed to send data: %d", err);
                return err;
            }
            continue;  // 超时，继续尝试
        }
        
        if (bytes_sent == 0) {
            INFRA_LOG_ERROR("Send returned 0 bytes");
            return INFRA_ERROR_IO;
        }

        total_sent += bytes_sent;
    }

    INFRA_LOG_INFO("%s: %zu bytes", direction, total_sent);
    return INFRA_OK;
}

// 单向转发线程参数
typedef struct {
    infra_socket_t src;                 // 源socket
    infra_socket_t dst;                 // 目标socket
    const char* direction;              // 转发方向
} forward_thread_param_t;

// 单向转发
static void* forward_thread(void* arg) {
    forward_thread_param_t* param = (forward_thread_param_t*)arg;
    if (!param) {
        return NULL;
    }

    // 设置非阻塞模式
    infra_net_set_nonblock(param->src, true);
    infra_net_set_nonblock(param->dst, true);

    char buffer[RINETD_BUFFER_SIZE];
    bool waiting_write = false;
    size_t write_pos = 0;
    size_t write_len = 0;
    
    while (g_context.running) {
        fd_set readfds, writefds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        
        if (!waiting_write) {
            FD_SET(infra_net_get_fd(param->src), &readfds);
        }
        if (waiting_write) {
            FD_SET(infra_net_get_fd(param->dst), &writefds);
        }

        struct timeval tv = {1, 0};  // 1秒超时，避免频繁轮询
        int max_fd = infra_net_get_fd(param->src);
        if (infra_net_get_fd(param->dst) > max_fd) {
            max_fd = infra_net_get_fd(param->dst);
        }

        int ready = select(max_fd + 1, &readfds, &writefds, NULL, &tv);
        if (ready < 0) {
            INFRA_LOG_ERROR("%s: Select error", param->direction);
            break;
        }
        if (ready == 0) {
            continue;
        }

        if (waiting_write && FD_ISSET(infra_net_get_fd(param->dst), &writefds)) {
            size_t bytes_sent = 0;
            infra_error_t err = infra_net_send(param->dst, buffer + write_pos, write_len, &bytes_sent);
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                continue;
            }
            if (err != INFRA_OK && err != INFRA_ERROR_TIMEOUT) {
                INFRA_LOG_DEBUG("%s: Send error: %d", param->direction, err);
                break;
            }
            if (bytes_sent > 0) {
                write_pos += bytes_sent;
                write_len -= bytes_sent;
                if (write_len == 0) {
                    waiting_write = false;
                    INFRA_LOG_INFO("%s: %zu bytes", param->direction, write_pos);
                }
            }
        }
        else if (!waiting_write && FD_ISSET(infra_net_get_fd(param->src), &readfds)) {
            size_t bytes_received = 0;
            infra_error_t err = infra_net_recv(param->src, buffer, RINETD_BUFFER_SIZE, &bytes_received);
            
            if (err == INFRA_ERROR_CLOSED || bytes_received == 0) {
                INFRA_LOG_DEBUG("%s: Connection closed", param->direction);
                // 先关闭写入端，让对端能收到FIN
                infra_net_shutdown(param->src, INFRA_NET_SHUTDOWN_WRITE);
                // 等待一小段时间，让数据发送完
                infra_sleep(100);
                // 然后完全关闭
                infra_net_close(param->src);
                break;
            }
            
            if (err != INFRA_OK && err != INFRA_ERROR_TIMEOUT) {
                INFRA_LOG_DEBUG("%s: Receive error: %d", param->direction, err);
                break;
            }
            
            if (bytes_received > 0) {
                size_t bytes_sent = 0;
                err = infra_net_send(param->dst, buffer, bytes_received, &bytes_sent);
                if (err == INFRA_ERROR_WOULD_BLOCK) {
                    waiting_write = true;
                    write_pos = bytes_sent;
                    write_len = bytes_received - bytes_sent;
                    continue;
                }
                if (err != INFRA_OK && err != INFRA_ERROR_TIMEOUT) {
                    INFRA_LOG_DEBUG("%s: Send error: %d", param->direction, err);
                    break;
                }
                if (bytes_sent > 0) {
                    INFRA_LOG_INFO("%s: %zu bytes", param->direction, bytes_sent);
                }
            }
        }
    }

    // 只关闭源socket，让对端的线程关闭目标socket
    infra_net_close(param->src);
    free(param);
    return NULL;
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

    // 创建客户端到服务器的转发线程参数
    forward_thread_param_t* c2s = malloc(sizeof(forward_thread_param_t));
    if (!c2s) {
        goto cleanup;
    }
    c2s->src = conn->client;
    c2s->dst = conn->server;
    c2s->direction = "Client to server";

    // 创建服务器到客户端的转发线程参数
    forward_thread_param_t* s2c = malloc(sizeof(forward_thread_param_t));
    if (!s2c) {
        free(c2s);
        goto cleanup;
    }
    s2c->src = conn->server;
    s2c->dst = conn->client;
    s2c->direction = "Server to client";

    // 启动转发线程
    infra_error_t err = infra_thread_pool_submit(g_context.pool, forward_thread, c2s);
    if (err != INFRA_OK) {
        free(c2s);
        free(s2c);
        goto cleanup;
    }

    err = infra_thread_pool_submit(g_context.pool, forward_thread, s2c);
    if (err != INFRA_OK) {
        free(s2c);
        goto cleanup;
    }

    // 主线程退出，转发线程会继续运行
    free(conn);
    return NULL;

cleanup:
    if (conn->client) {
        infra_net_close(conn->client);
    }
    if (conn->server) {
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

    if (g_context.listener) {
        infra_net_close(g_context.listener);
        g_context.listener = NULL;
    }

    return INFRA_OK;
}

static infra_error_t create_listener(void) {
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
    addr.host = g_context.rule.src_addr;
    addr.port = g_context.rule.src_port;
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

    g_context.listener = listener;
    return INFRA_OK;
}

infra_error_t rinetd_start(void) {
    if (g_context.running) {
        return INFRA_ERROR_BUSY;
    }

    // 创建监听器
    infra_error_t err = create_listener();
    if (err != INFRA_OK) {
        return err;
    }

    // 设置运行标志
    g_context.running = true;

    // 在前台运行
    INFRA_LOG_INFO("Starting rinetd service in foreground");
    while (g_context.running) {
        // 等待连接
        infra_socket_t client = NULL;
        infra_net_addr_t client_addr = {0};
        err = infra_net_accept(g_context.listener, &client, &client_addr);
        if (err != INFRA_OK) {
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                continue;
            }
            INFRA_LOG_ERROR("Failed to accept connection: %d", err);
            break;
        }

        INFRA_LOG_INFO("Accepted connection from %s:%d", 
            client_addr.host, client_addr.port);

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
        addr.host = g_context.rule.dst_addr;
        addr.port = g_context.rule.dst_port;
        err = infra_net_connect(&addr, &server, &config);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to connect to server: %d", err);
            infra_net_close(client);
            infra_net_close(server);
            continue;
        }

        INFRA_LOG_INFO("Connected to server %s:%d", 
            addr.host, addr.port);

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
        conn->rule = &g_context.rule;

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

    return INFRA_OK;
}

infra_error_t rinetd_stop(void) {
    if (!g_context.running) {
        return INFRA_OK;
    }

    g_context.running = false;
    
    if (g_context.listener) {
        infra_net_close(g_context.listener);
        g_context.listener = NULL;
    }

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

    // 尝试加载配置文件
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        INFRA_LOG_ERROR("Failed to open config file: %s", path);
        return INFRA_ERROR_IO;
    }

    // 解析配置文件
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        // 跳过注释和空行
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        // 解析规则
        char src_addr[RINETD_MAX_ADDR_LEN] = {0};
        char dst_addr[RINETD_MAX_ADDR_LEN] = {0};
        int src_port = 0;
        int dst_port = 0;

        if (sscanf(line, "%s %d %s %d", src_addr, &src_port, dst_addr, &dst_port) == 4) {
            strncpy(g_context.rule.src_addr, src_addr, RINETD_MAX_ADDR_LEN - 1);
            g_context.rule.src_port = src_port;
            strncpy(g_context.rule.dst_addr, dst_addr, RINETD_MAX_ADDR_LEN - 1);
            g_context.rule.dst_port = dst_port;
            INFRA_LOG_INFO("Loaded rule: %s:%d -> %s:%d", 
                src_addr, src_port, dst_addr, dst_port);
            break;  // 只使用第一条规则
        }
    }

    fclose(fp);
    return INFRA_OK;
} 

