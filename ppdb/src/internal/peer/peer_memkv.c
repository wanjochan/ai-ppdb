#include "internal/peer/peer_service.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_sync.h"
#include "internal/infra/infra_memory.h"
#include "internal/poly/poly_memkv.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

#define MEMKV_VERSION          "1.0.0"
#define MEMKV_BUFFER_SIZE      8192
#define MEMKV_MAX_KEY_SIZE     250
#define MEMKV_MAX_VALUE_SIZE   (1024 * 1024)  // 1MB
#define MEMKV_MIN_THREADS      32
#define MEMKV_MAX_THREADS      512
#define MEMKV_DEFAULT_PORT     11211

//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------

// 连接上下文
typedef struct memkv_conn {
    infra_socket_t sock;           // 客户端socket
    char buffer[MEMKV_BUFFER_SIZE];  // 数据缓冲区
    infra_net_addr_t addr;         // 客户端地址
} memkv_conn_t;

// 服务上下文
typedef struct memkv_context {
    bool running;
    uint16_t port;
    infra_thread_pool_t* thread_pool;
    infra_mutex_t mutex;
    poly_memkv_db_t* store;
    poly_memkv_engine_t engine;
    char* plugin_path;
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

// Forward declarations
static infra_error_t poly_memkv_counter_op(poly_memkv_db_t* db, const char* key, int64_t delta, int64_t* new_value);
static infra_error_t poly_memkv_incr(poly_memkv_db_t* db, const char* key, int64_t delta, int64_t* new_value);
static infra_error_t poly_memkv_decr(poly_memkv_db_t* db, const char* key, int64_t delta, int64_t* new_value);

//-----------------------------------------------------------------------------
// Command Line Options
//-----------------------------------------------------------------------------

// 命令行选项
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
static void handle_client(int client_fd) {
    char buffer[MEMKV_BUFFER_SIZE];
    char* cmd_start = buffer;
    ssize_t bytes_read;
    size_t buffer_used = 0;
    
    while (g_context.running) {
        // 读取命令
        bytes_read = recv(client_fd, buffer + buffer_used, 
                         sizeof(buffer) - buffer_used - 1, 0);
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                // 客户端关闭连接
                break;
            }
            if (errno == EINTR) {
                // 被信号中断，继续读取
                continue;
            }
            // 其他错误
            break;
        }

        buffer_used += bytes_read;
        buffer[buffer_used] = '\0';
        
        // 处理所有完整的命令
        while (cmd_start < buffer + buffer_used) {
            char* cmd_end = strstr(cmd_start, "\r\n");
            if (!cmd_end) {
                // 命令不完整，等待更多数据
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
            
            // 处理不同类型的命令
            if (sscanf(cmd_start, "set %s %s %s %s", key, flags_str, exptime_str, bytes_str) == 4) {
                // SET 命令
                value_len = atoi(bytes_str);
                if (value_len > MEMKV_MAX_VALUE_SIZE) {
                    const char* error = "CLIENT_ERROR value too large\r\n";
                    send(client_fd, error, strlen(error), 0);
                    goto next_command;
                }
                
                // 检查是否有足够的数据
                char* value_start = cmd_end + 2;
                if (value_start + value_len + 2 > buffer + buffer_used) {
                    // 数据不完整，等待更多数据
                    *cmd_end = '\r';  // 恢复 \r\n
                    break;
                }
                
                // 验证数据块结尾的 \r\n
                if (value_start[value_len] != '\r' || value_start[value_len + 1] != '\n') {
                    const char* error = "CLIENT_ERROR bad data chunk\r\n";
                    send(client_fd, error, strlen(error), 0);
                    goto next_command;
                }
                
                // 存储值
                infra_error_t err = poly_memkv_set(g_context.store, key, value_start, value_len);
                if (err == INFRA_OK) {
                    send(client_fd, "STORED\r\n", 8, 0);
                } else {
                    send(client_fd, "SERVER_ERROR\r\n", 14, 0);
                }
                
                cmd_start = value_start + value_len + 2;  // 跳过值和结尾的 \r\n
                
            } else if (sscanf(cmd_start, "get %s", key) == 1) {
                // GET 命令
                void* value;
                size_t value_len;
                infra_error_t err = poly_memkv_get(g_context.store, key, &value, &value_len);
                
                if (err == INFRA_OK) {
                    char response[32];
                    int len = snprintf(response, sizeof(response), 
                                     "VALUE %s 0 %zu\r\n", key, value_len);
                    send(client_fd, response, len, 0);
                    send(client_fd, value, value_len, 0);
                    send(client_fd, "\r\n", 2, 0);
                    infra_free(value);
                }
                send(client_fd, "END\r\n", 5, 0);
                
            } else if (sscanf(cmd_start, "delete %s", key) == 1) {
                // DELETE 命令
                infra_error_t err = poly_memkv_del(g_context.store, key);
                if (err == INFRA_OK) {
                    send(client_fd, "DELETED\r\n", 9, 0);
                } else {
                    send(client_fd, "NOT_FOUND\r\n", 11, 0);
                }
                
            } else if (strncmp(cmd_start, "flush_all", 9) == 0) {
                // FLUSH_ALL 命令 - 清空所有数据
                // 创建新的存储实例
                poly_memkv_config_t config = {
                    .engine = POLY_MEMKV_ENGINE_SQLITE,
                    .url = ":memory:",
                    .max_key_size = MEMKV_MAX_KEY_SIZE,
                    .max_value_size = MEMKV_MAX_VALUE_SIZE,
                    .memory_limit = 0,
                    .enable_compression = false,
                    .plugin_path = NULL,
                    .allow_fallback = true,
                    .read_only = false
                };
                
                poly_memkv_db_t* new_store = NULL;
                infra_error_t err = poly_memkv_create(&config, &new_store);
                if (err == INFRA_OK) {
                    poly_memkv_db_t* old_store = g_context.store;
                    g_context.store = new_store;
                    poly_memkv_destroy(old_store);
                    send(client_fd, "OK\r\n", 4, 0);
                } else {
                    send(client_fd, "SERVER_ERROR\r\n", 14, 0);
                }
                
            } else {
                // 未知命令
                send(client_fd, "ERROR\r\n", 7, 0);
            }
            
next_command:
            cmd_start = cmd_end + 2;
        }
        
        // 移动未完成的命令到缓冲区开始
        if (cmd_start < buffer + buffer_used) {
            buffer_used = buffer + buffer_used - cmd_start;
            memmove(buffer, cmd_start, buffer_used);
            cmd_start = buffer;
        } else {
            buffer_used = 0;
            cmd_start = buffer;
        }
    }
    
    close(client_fd);
}

// 服务线程
static infra_error_t service_thread(void) {
    infra_error_t err;
    infra_socket_t listen_sock;
    infra_config_t config = {0};

    // 创建监听套接字
    err = infra_net_create(&listen_sock, false, &config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create listen socket: %d", err);
        return err;
    }

    // 设置地址重用
    err = infra_net_set_reuseaddr(listen_sock, true);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set reuseaddr: %d", err);
        infra_net_close(listen_sock);
        return err;
    }

    // 绑定地址
    infra_net_addr_t addr = {
        .host = "127.0.0.1",
        .port = g_context.port
    };
    err = infra_net_bind(listen_sock, &addr);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to bind address: %d", err);
        infra_net_close(listen_sock);
        return err;
    }

    // 开始监听
    err = infra_net_listen(listen_sock);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to listen: %d", err);
        infra_net_close(listen_sock);
        return err;
    }

    INFRA_LOG_INFO("MemKV service listening on port %d", g_context.port);

    // 设置监听套接字为非阻塞模式
    err = infra_net_set_nonblock(listen_sock, true);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to set nonblock mode: %d", err);
        infra_net_close(listen_sock);
        return err;
    }

    // 主循环
    while (g_context.running) {
        // 定期检查存储引擎状态
        static int check_count = 0;
        if (++check_count >= 100) {  // 每100次循环检查一次
            check_count = 0;
            void* data = NULL;
            size_t data_len = 0;
            err = poly_memkv_get(g_context.store, "__test_key__", &data, &data_len);
            if (err == INFRA_ERROR_SYSTEM) {
                INFRA_LOG_ERROR("Storage engine failure detected");
                g_context.running = false;
                break;
            }
            if (data) infra_free(data);
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(infra_net_get_fd(listen_sock), &readfds);

        // 设置超时
        struct timeval tv = {.tv_sec = 1, .tv_usec = 0};  // 1秒超时
        int max_fd = infra_net_get_fd(listen_sock);

        int ready = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (ready < 0) {
            if (errno == EINTR) {
                // 被信号中断，检查运行状态
                if (!g_context.running) {
                    break;
                }
                continue;
            }
            INFRA_LOG_ERROR("Failed to select: %d", errno);
            continue;  // 继续尝试
        }
        if (ready == 0) {
            // 超时，检查运行状态
            if (!g_context.running) {
                break;
            }
            continue;
        }

        // 检查是否有新连接
        if (FD_ISSET(infra_net_get_fd(listen_sock), &readfds)) {
            // 接受新连接
            infra_socket_t client_sock;
            infra_net_addr_t client_addr;
            err = infra_net_accept(listen_sock, &client_sock, &client_addr);
            if (err != INFRA_OK) {
                if (err == INFRA_ERROR_WOULD_BLOCK || err == INFRA_ERROR_TIMEOUT) {
                    continue;
                }
                INFRA_LOG_ERROR("Failed to accept connection: %d", err);
                continue;
            }

            INFRA_LOG_INFO("Accepted connection from %s:%d", client_addr.host, client_addr.port);

            // 处理客户端连接
            handle_client(infra_net_get_fd(client_sock));
        }
    }

    // 关闭监听套接字
    infra_net_close(listen_sock);
    INFRA_LOG_INFO("Service stopped");
    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Service Implementation
//-----------------------------------------------------------------------------

// 初始化服务
static infra_error_t memkv_init(const infra_config_t* config) {
    if (!config) return INFRA_ERROR_INVALID_PARAM;

    // 初始化上下文
    g_context.running = false;
    g_context.port = MEMKV_DEFAULT_PORT;

    // 创建存储实例
    poly_memkv_config_t store_config = {
        .engine = POLY_MEMKV_ENGINE_SQLITE,
        .url = ":memory:",  // 修复 SQLite URL
        .max_key_size = MEMKV_MAX_KEY_SIZE,
        .max_value_size = MEMKV_MAX_VALUE_SIZE,
        .memory_limit = 0,
        .enable_compression = false,
        .plugin_path = NULL,
        .allow_fallback = true,
        .read_only = false
    };

    return poly_memkv_create(&store_config, &g_context.store);
}

// 启动服务
static infra_error_t memkv_start(void) {
    if (!g_context.store) {
        INFRA_LOG_ERROR("Service not initialized");
        return INFRA_ERROR_NOT_READY;
    }
    
    if (g_context.running) {
        INFRA_LOG_ERROR("Service already running");
        return INFRA_ERROR_ALREADY_EXISTS;
    }
    
    g_context.running = true;
    
    // TODO: 实现守护进程模式
    // 目前服务在前台运行，后续需要:
    // 1. 添加 --daemon 选项支持后台运行
    // 2. 实现 pid 文件管理
    // 3. 实现日志重定向
    // 4. 实现信号处理
    
    // 在前台运行服务
    INFRA_LOG_INFO("Starting memkv service in foreground on port %d", g_context.port);
    infra_error_t err = service_thread();
    
    // 确保服务正常停止
    g_context.running = false;
    
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Service thread failed: %d", err);
        return err;
    }
    
    INFRA_LOG_INFO("Service stopped normally");
    return INFRA_OK;
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

// 清理服务
static infra_error_t memkv_cleanup(void) {
    if (g_context.running) {
        memkv_stop();
    }

    if (g_context.store) {
        poly_memkv_destroy(g_context.store);
        g_context.store = NULL;
    }
    
    return INFRA_OK;
}

// 检查服务是否运行
bool memkv_is_running(void) {
    return g_context.running;
}

// 处理命令行
infra_error_t memkv_cmd_handler(int argc, char* argv[]) {
    static bool initialized = false;
    infra_error_t err;
    
    if (argc < 1) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    
    // 解析命令行参数
    bool should_start = false;
    uint16_t new_port = 11211;  // 默认端口
    poly_memkv_engine_t new_engine = POLY_MEMKV_ENGINE_SQLITE;  // 默认引擎
    char* new_plugin_path = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--start") == 0) {
            should_start = true;
        } else if (strcmp(argv[i], "--stop") == 0) {
            if (!initialized) {
                INFRA_LOG_INFO("Service is not initialized");
                return INFRA_OK;
            }
            g_memkv_service.state = SERVICE_STATE_STOPPING;
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
            initialized = false;
            g_memkv_service.state = SERVICE_STATE_STOPPED;
            INFRA_LOG_INFO("MemKV service stopped successfully");
            return INFRA_OK;
        } else if (strcmp(argv[i], "--status") == 0) {
            if (!initialized) {
                INFRA_LOG_INFO("Service is not initialized");
                return INFRA_OK;
            }
            if (g_context.running) {
                INFRA_LOG_INFO("Service is running on port %d with %s engine",
                    g_context.port,
                    g_context.engine == POLY_MEMKV_ENGINE_SQLITE ? "sqlite" : "duckdb");
            } else {
                INFRA_LOG_INFO("Service is stopped");
            }
            return INFRA_OK;
        } else if (strncmp(argv[i], "--port=", 7) == 0) {
            int port = atoi(argv[i] + 7);
            if (port <= 0 || port > 65535) {
                INFRA_LOG_ERROR("Invalid port number: %d", port);
                return INFRA_ERROR_INVALID_PARAM;
            }
            new_port = port;
        } else if (strncmp(argv[i], "--engine=", 9) == 0) {
            const char* engine = argv[i] + 9;
            if (strcmp(engine, "sqlite") == 0) {
                new_engine = POLY_MEMKV_ENGINE_SQLITE;
            } else if (strcmp(engine, "duckdb") == 0) {
                new_engine = POLY_MEMKV_ENGINE_DUCKDB;
            } else {
                INFRA_LOG_ERROR("Invalid engine type: %s", engine);
                return INFRA_ERROR_INVALID_PARAM;
            }
        } else if (strncmp(argv[i], "--plugin=", 9) == 0) {
            const char* new_path = argv[i] + 9;
            new_plugin_path = strdup(new_path);
            if (!new_plugin_path) {
                return INFRA_ERROR_NO_MEMORY;
            }
        }
    }
    
    // 如果需要启动且服务在运行，则重启
    if (should_start && g_context.running) {
        INFRA_LOG_INFO("Service is running, restarting...");
        err = memkv_stop();
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to stop service for restart: %d", err);
            if (new_plugin_path) free(new_plugin_path);
            return err;
        }
        err = memkv_cleanup();
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to cleanup service for restart: %d", err);
            if (new_plugin_path) free(new_plugin_path);
            return err;
        }
        initialized = false;
    }
    
    // 如果需要启动服务
    if (should_start) {
        // 确保服务已初始化
        if (!initialized) {
            infra_config_t config = INFRA_DEFAULT_CONFIG;
            
            // 在初始化前设置配置
            g_context.port = new_port;
            g_context.engine = new_engine;
            if (g_context.plugin_path) {
                free(g_context.plugin_path);
            }
            g_context.plugin_path = new_plugin_path;
            
            err = memkv_init(&config);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to initialize memkv service: %d", err);
                return err;
            }
            initialized = true;
            g_memkv_service.state = SERVICE_STATE_STARTING;
        }
        
        // 启动服务
        err = memkv_start();
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to start memkv service: %d", err);
            g_memkv_service.state = SERVICE_STATE_STOPPED;
            return err;
        }
        
        g_memkv_service.state = SERVICE_STATE_RUNNING;
        INFRA_LOG_INFO("MemKV service started successfully");
    } else if (new_plugin_path) {
        free(new_plugin_path);  // 如果不启动服务，释放新分配的插件路径
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
    if (!db) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    return poly_memkv_get(db, key, value, value_len);
}

infra_error_t peer_memkv_set(poly_memkv_db_t* db, const char* key, const void* value, size_t value_len) {
    if (!db) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    return poly_memkv_set(db, key, value, value_len);
}

infra_error_t peer_memkv_del(poly_memkv_db_t* db, const char* key) {
    if (!db) {
        return INFRA_ERROR_INVALID_PARAM;
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
    if (!iter) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    return poly_memkv_iter_next(iter, key, value, value_len);
}

void peer_memkv_iter_destroy(poly_memkv_iter_t* iter) {
    if (iter) {
        poly_memkv_iter_destroy(iter);
    }
}

// 实现 incr/decr 功能
static infra_error_t poly_memkv_counter_op(poly_memkv_db_t* db, const char* key, int64_t delta, int64_t* new_value) {
    void* value = NULL;
    size_t value_len = 0;
    infra_error_t err;
    
    // 获取当前值
    err = poly_memkv_get(db, key, &value, &value_len);
    if (err != INFRA_OK && err != INFRA_ERROR_NOT_FOUND) {
        return err;
    }
    
    int64_t current = 0;
    if (err == INFRA_OK && value) {
        // 转换为数字
        char* end;
        current = strtoll(value, &end, 10);
        if (*end != '\0') {
            free(value);
            return INFRA_ERROR_INVALID_FORMAT;
        }
        free(value);
    }
    
    // 计算新值
    current += delta;
    *new_value = current;
    
    // 转换回字符串并存储
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)current);
    return poly_memkv_set(db, key, buf, strlen(buf));
}

static infra_error_t poly_memkv_incr(poly_memkv_db_t* db, const char* key, int64_t delta, int64_t* new_value) {
    return poly_memkv_counter_op(db, key, delta, new_value);
}

static infra_error_t poly_memkv_decr(poly_memkv_db_t* db, const char* key, int64_t delta, int64_t* new_value) {
    return poly_memkv_counter_op(db, key, -delta, new_value);
}
