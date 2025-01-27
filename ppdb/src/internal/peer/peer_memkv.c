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
    poly_memkv_t* store;
    poly_memkv_engine_type_t engine;
    char* plugin_path;
} memkv_context_t;

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

// 读取命令
static infra_error_t read_command(memkv_conn_t* conn, char* cmd, size_t cmd_size,
    char* key, size_t key_size, char* data, size_t data_size,
    size_t* data_len, uint32_t* flags, uint32_t* exptime) {
    size_t pos = 0;
    bool found_cr = false;
    infra_error_t err;

    // 读取命令行直到 \r\n
    while (pos < sizeof(conn->buffer) - 1) {
        size_t recv_size = 0;
        err = infra_net_recv(conn->sock, conn->buffer + pos, 1, &recv_size);
        if (err != INFRA_OK) {
            if (err == INFRA_ERROR_WOULD_BLOCK || err == INFRA_ERROR_TIMEOUT) {
                return err;  // 让外层循环处理非阻塞
            }
            return err;
        }
        if (recv_size == 0) {
            return INFRA_ERROR_CLOSED;
        }

        if (conn->buffer[pos] == '\r') {
            found_cr = true;
        } else if (found_cr && conn->buffer[pos] == '\n') {
            conn->buffer[pos - 1] = '\0';  // 去掉 \r\n
            break;
        } else {
            found_cr = false;
        }
        pos++;
    }

    // 解析命令行
    char* token = strtok(conn->buffer, " ");
    if (!token) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    strncpy(cmd, token, cmd_size - 1);
    cmd[cmd_size - 1] = '\0';

    // 解析 key
    token = strtok(NULL, " ");
    if (!token) {
        return INFRA_ERROR_INVALID_PARAM;
    }
    strncpy(key, token, key_size - 1);
    key[key_size - 1] = '\0';

    // 如果是 set 命令，继续解析 flags、exptime 和 bytes
    if (strcasecmp(cmd, "set") == 0) {
        // 解析 flags
        token = strtok(NULL, " ");
        if (!token || sscanf(token, "%u", flags) != 1) {
            return INFRA_ERROR_INVALID_PARAM;
        }

        // 解析 exptime
        token = strtok(NULL, " ");
        if (!token || sscanf(token, "%u", exptime) != 1) {
            return INFRA_ERROR_INVALID_PARAM;
        }

        // 解析 bytes
        token = strtok(NULL, " ");
        if (!token || sscanf(token, "%zu", data_len) != 1) {
            return INFRA_ERROR_INVALID_PARAM;
        }

        // 检查数据长度
        if (*data_len >= data_size) {
            return INFRA_ERROR_NO_MEMORY;
        }

        // 读取数据
        size_t bytes_read = 0;
        while (bytes_read < *data_len) {
            size_t recv_size = 0;
            err = infra_net_recv(conn->sock, data + bytes_read, *data_len - bytes_read, &recv_size);
            if (err != INFRA_OK) {
                if (err == INFRA_ERROR_WOULD_BLOCK || err == INFRA_ERROR_TIMEOUT) {
                    return err;  // 让外层循环处理非阻塞
                }
                return err;
            }
            if (recv_size == 0) {
                return INFRA_ERROR_CLOSED;
            }
            bytes_read += recv_size;
        }

        // 读取结尾的 \r\n
        char end[2];
        size_t recv_size = 0;
        err = infra_net_recv(conn->sock, end, 2, &recv_size);
        if (err != INFRA_OK) {
            if (err == INFRA_ERROR_WOULD_BLOCK || err == INFRA_ERROR_TIMEOUT) {
                return err;  // 让外层循环处理非阻塞
            }
            return err;
        }
        if (recv_size != 2 || end[0] != '\r' || end[1] != '\n') {
            return INFRA_ERROR_INVALID_PARAM;
        }
    } else if (strcasecmp(cmd, "delete") == 0) {
        // delete 命令可能有额外的 noreply 参数，忽略它
        token = strtok(NULL, " ");  // 尝试读取 noreply
    } else if (strcasecmp(cmd, "incr") == 0 || strcasecmp(cmd, "decr") == 0) {
        // 解析增量值
        token = strtok(NULL, " ");
        if (!token || sscanf(token, "%zu", data_len) != 1) {
            *data_len = 1;  // 默认增量为 1
        }
    }

    return INFRA_OK;
}

// 发送响应
static void send_response(memkv_conn_t* conn, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(conn->buffer, sizeof(conn->buffer) - 2, fmt, args);
    va_end(args);

    if (len < 0 || len >= sizeof(conn->buffer) - 2) return;

    conn->buffer[len] = '\r';
    conn->buffer[len + 1] = '\n';
    infra_net_send(conn->sock, conn->buffer, len + 2, NULL);
}

// 处理连接
static void* handle_connection(void* arg) {
    memkv_conn_t* conn = (memkv_conn_t*)arg;
    if (!conn || !conn->sock) {
        if (conn) free(conn);
        return NULL;
    }

    char cmd[32];
    char key[256];
    char value[65536];
    size_t value_len;
    uint32_t flags = 0;
    uint32_t exptime = 0;

    // 设置socket超时（30秒）
    infra_net_set_timeout(conn->sock, 30000);

    while (g_context.running) {
        // 读取命令
        infra_error_t err = read_command(conn, cmd, sizeof(cmd), key, sizeof(key),
            value, sizeof(value), &value_len, &flags, &exptime);
        if (err != INFRA_OK) break;

        // 处理命令
        if (strcasecmp(cmd, "get") == 0) {
            void* data = NULL;
            size_t data_len;
            if (poly_memkv_get(g_context.store, key, &data, &data_len) == INFRA_OK && data) {
                send_response(conn, "VALUE %s %u %zu", key, flags, data_len);
                infra_net_send(conn->sock, data, data_len, NULL);
                send_response(conn, "");
                send_response(conn, "END");
                free(data);
            } else {
                send_response(conn, "END");
            }
        }
        else if (strcasecmp(cmd, "set") == 0) {
            err = poly_memkv_set(g_context.store, key, value, value_len);
            send_response(conn, err == INFRA_OK ? "STORED" : "NOT_STORED");
        }
        else if (strcasecmp(cmd, "delete") == 0) {
            err = poly_memkv_del(g_context.store, key);
            send_response(conn, err == INFRA_OK ? "DELETED" : "NOT_FOUND");
        }
        else if (strcasecmp(cmd, "incr") == 0 || strcasecmp(cmd, "decr") == 0) {
            uint64_t new_value;
            if (strcasecmp(cmd, "incr") == 0) {
                err = poly_memkv_incr(g_context.store, key, value_len, &new_value);
            } else {
                err = poly_memkv_decr(g_context.store, key, value_len, &new_value);
            }
            send_response(conn, err == INFRA_OK ? "%lu" : "NOT_FOUND", new_value);
        }
        else if (strcasecmp(cmd, "flush_all") == 0) {
            send_response(conn, "OK");
        }
        else {
            send_response(conn, "ERROR");
        }
    }

    infra_net_close(conn->sock);
    free(conn);
    return NULL;
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
            // 创建连接结构
            memkv_conn_t* conn = malloc(sizeof(memkv_conn_t));
            if (!conn) {
                INFRA_LOG_ERROR("Failed to allocate connection");
                continue;
            }
            memset(conn, 0, sizeof(memkv_conn_t));  // 清零内存

            // 接受新连接
            err = infra_net_accept(listen_sock, &conn->sock, &conn->addr);
            if (err != INFRA_OK) {
                if (err == INFRA_ERROR_WOULD_BLOCK || err == INFRA_ERROR_TIMEOUT) {
                    free(conn);
                    continue;
                }
                INFRA_LOG_ERROR("Failed to accept connection: %d", err);
                free(conn);
                continue;
            }

            INFRA_LOG_INFO("Accepted connection from %s:%d", conn->addr.host, conn->addr.port);

            // 提交到线程池处理
            err = infra_thread_pool_submit(g_context.thread_pool, handle_connection, conn);
            if (err != INFRA_OK) {
                INFRA_LOG_ERROR("Failed to submit connection to thread pool: %d", err);
                infra_net_close(conn->sock);
                free(conn);
                continue;
            }
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
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 如果已经初始化过，先清理
    if (g_context.store) {
        memkv_cleanup();
    }

    // 保存现有配置
    uint16_t port = g_context.port ? g_context.port : 11211;  // 如果未设置则使用默认值
    poly_memkv_engine_type_t engine = g_context.engine ? g_context.engine : POLY_MEMKV_ENGINE_SQLITE;  // 默认引擎
    char* plugin_path = g_context.plugin_path;  // 保存插件路径
    g_context.plugin_path = NULL;  // 防止被 memset 清除后释放
    
    // 初始化上下文
    memset(&g_context, 0, sizeof(g_context));
    g_context.port = port;  // 恢复端口
    g_context.engine = engine;  // 恢复引擎类型
    g_context.plugin_path = plugin_path;  // 恢复插件路径
    
    // 创建线程池
    infra_thread_pool_config_t pool_config = {
        .min_threads = MEMKV_MIN_THREADS,
        .max_threads = MEMKV_MAX_THREADS,
        .queue_size = MEMKV_MAX_THREADS * 2  // 设置队列大小
    };
    infra_error_t err = infra_thread_pool_create(&pool_config, &g_context.thread_pool);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create thread pool: %d", err);
        return err;
    }
    
    // 创建互斥锁
    err = infra_mutex_create(&g_context.mutex);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create mutex: %d", err);
        infra_thread_pool_destroy(g_context.thread_pool);
        return err;
    }
    
    // 创建存储实例
    poly_memkv_config_t config_memkv = {
        .max_key_size = MEMKV_MAX_KEY_SIZE,
        .max_value_size = MEMKV_MAX_VALUE_SIZE,
        .engine_type = g_context.engine,
        .plugin_path = g_context.plugin_path
    };
    
    err = poly_memkv_create(&config_memkv, &g_context.store);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to create store: %d", err);
        infra_mutex_destroy(&g_context.mutex);
        infra_thread_pool_destroy(g_context.thread_pool);
        return err;
    }
    
    INFRA_LOG_INFO("MemKV service initialized with port %d and %s engine",
        g_context.port,
        g_context.engine == POLY_MEMKV_ENGINE_SQLITE ? "sqlite" : "duckdb");
    
    return INFRA_OK;
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
    
    if (g_context.thread_pool) {
        infra_thread_pool_destroy(g_context.thread_pool);
        g_context.thread_pool = NULL;
    }
    
    if (g_context.plugin_path) {
        free(g_context.plugin_path);
        g_context.plugin_path = NULL;
    }
    
    infra_mutex_destroy(&g_context.mutex);
    
    memset(&g_context, 0, sizeof(g_context));
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
    poly_memkv_engine_type_t new_engine = POLY_MEMKV_ENGINE_SQLITE;  // 默认引擎
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

