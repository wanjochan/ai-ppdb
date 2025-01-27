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
static memkv_context_t g_context;

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

// 读取命令
static infra_error_t read_command(infra_socket_t sock, char* cmd, size_t cmd_size,
    char* key, size_t key_size, char* value, size_t value_size, size_t* value_len) {
    char line[1024];
    size_t line_len;
    
    // 读取命令行
    infra_error_t err = infra_net_recv(sock, line, sizeof(line), &line_len);
    if (err != INFRA_OK) {
        return err;
    }
    
    // 解析命令
    char* token = strtok(line, " \r\n");
    if (!token) {
        return INFRA_ERROR_INVALID_FORMAT;
    }
    strncpy(cmd, token, cmd_size);
    
    // 解析键
    token = strtok(NULL, " \r\n");
    if (!token) {
        return INFRA_ERROR_INVALID_FORMAT;
    }
    strncpy(key, token, key_size);
    
    // 如果是 SET 命令，还需要读取值
    if (strcmp(cmd, "SET") == 0) {
        // 读取值大小
        token = strtok(NULL, " \r\n");
        if (!token) {
            return INFRA_ERROR_INVALID_FORMAT;
        }
        *value_len = atoi(token);
        
        if (*value_len > value_size) {
            return INFRA_ERROR_NO_SPACE;
        }
        
        // 读取值
        err = infra_net_recv(sock, value, *value_len, NULL);
        if (err != INFRA_OK) {
            return err;
        }
        
        // 读取结束标记
        char end[2];
        err = infra_net_recv(sock, end, 2, NULL);
        if (err != INFRA_OK) {
            return err;
        }
        if (end[0] != '\r' || end[1] != '\n') {
            return INFRA_ERROR_INVALID_FORMAT;
        }
    }
    
    return INFRA_OK;
}

// 发送响应
static infra_error_t send_response(infra_socket_t sock, const char* response) {
    size_t len = strlen(response);
    size_t sent = 0;
    return infra_net_send(sock, response, len, &sent);
}

// 处理连接
static void* handle_connection(void* arg) {
    infra_socket_t sock = (infra_socket_t)arg;
    char cmd[32];
    char key[256];
    char value[65536];
    size_t value_len;
    char response[1024];
    infra_error_t err;
    
    while (true) {
        // 读取命令
        err = read_command(sock, cmd, sizeof(cmd), key, sizeof(key),
            value, sizeof(value), &value_len);
        if (err != INFRA_OK) {
            break;
        }
        
        // 处理命令
        if (strcmp(cmd, "GET") == 0) {
            void* data = NULL;
            size_t data_size = 0;
            
            // 获取值
            err = poly_memkv_get(g_context.store, key, &data, &data_size);
            if (err == INFRA_OK) {
                // 发送响应
                snprintf(response, sizeof(response), "VALUE %zu\r\n", data_size);
                send_response(sock, response);
                send_response(sock, data);
                send_response(sock, "\r\n");
                free(data);
            } else {
                send_response(sock, "NOT_FOUND\r\n");
            }
        }
        else if (strcmp(cmd, "SET") == 0) {
            // 设置值
            err = poly_memkv_set(g_context.store, key, value, value_len);
            if (err == INFRA_OK) {
                send_response(sock, "STORED\r\n");
            } else {
                send_response(sock, "NOT_STORED\r\n");
            }
        }
        else if (strcmp(cmd, "DELETE") == 0) {
            // 删除值
            err = poly_memkv_del(g_context.store, key);
            if (err == INFRA_OK) {
                send_response(sock, "DELETED\r\n");
            } else {
                send_response(sock, "NOT_FOUND\r\n");
            }
        }
        else if (strcmp(cmd, "STATS") == 0) {
            // 获取统计信息
            const poly_memkv_stats_t* stats = poly_memkv_get_stats(g_context.store);
            snprintf(response, sizeof(response),
                "STAT cmd_get %lu\r\n"
                "STAT cmd_set %lu\r\n"
                "STAT get_hits %lu\r\n"
                "STAT get_misses %lu\r\n"
                "STAT curr_items %lu\r\n"
                "STAT total_items %lu\r\n"
                "STAT bytes %lu\r\n"
                "END\r\n",
                (unsigned long)poly_atomic_get((poly_atomic_t*)&stats->cmd_get),
                (unsigned long)poly_atomic_get((poly_atomic_t*)&stats->cmd_set),
                (unsigned long)poly_atomic_get((poly_atomic_t*)&stats->hits),
                (unsigned long)poly_atomic_get((poly_atomic_t*)&stats->misses),
                (unsigned long)poly_atomic_get((poly_atomic_t*)&stats->curr_items),
                (unsigned long)poly_atomic_get((poly_atomic_t*)&stats->total_items),
                (unsigned long)poly_atomic_get((poly_atomic_t*)&stats->bytes));
            send_response(sock, response);
        }
        else if (strcmp(cmd, "QUIT") == 0) {
            break;
        }
        else {
            send_response(sock, "ERROR\r\n");
        }
    }
    
    infra_net_close(sock);
    return NULL;
}

// 服务线程
static void* service_thread(void* arg) {
    infra_socket_t listen_sock;
    infra_error_t err;
    
    // 创建监听套接字
    err = infra_net_create(&listen_sock, false, NULL);
    if (err != INFRA_OK) {
        return NULL;
    }
    
    // 绑定地址
    infra_net_addr_t addr = {
        .host = "127.0.0.1",
        .port = g_context.port
    };
    err = infra_net_bind(listen_sock, &addr);
    if (err != INFRA_OK) {
        infra_net_close(listen_sock);
        return NULL;
    }
    
    // 开始监听
    err = infra_net_listen(listen_sock);
    if (err != INFRA_OK) {
        infra_net_close(listen_sock);
        return NULL;
    }
    
    // 接受连接
    while (g_context.running) {
        infra_socket_t client_sock;
        infra_net_addr_t client_addr;
        err = infra_net_accept(listen_sock, &client_sock, &client_addr);
        if (err != INFRA_OK) {
            continue;
        }
        
        // 提交到线程池处理
        err = infra_thread_pool_submit(g_context.thread_pool,
            (infra_thread_func_t)handle_connection, (void*)client_sock);
        if (err != INFRA_OK) {
            infra_net_close(client_sock);
        }
    }
    
    infra_net_close(listen_sock);
    return NULL;
}

//-----------------------------------------------------------------------------
// Service Implementation
//-----------------------------------------------------------------------------

// 初始化服务
static infra_error_t memkv_init(const infra_config_t* config) {
    infra_error_t err;
    
    // 初始化上下文
    memset(&g_context, 0, sizeof(g_context));
    g_context.port = 11211;
    g_context.engine = POLY_MEMKV_ENGINE_SQLITE;
    
    // 创建线程池
    infra_thread_pool_config_t pool_config = {
        .min_threads = MEMKV_MIN_THREADS,
        .max_threads = MEMKV_MAX_THREADS
    };
    err = infra_thread_pool_create(&pool_config, &g_context.thread_pool);
    if (err != INFRA_OK) {
        return err;
    }
    
    // 创建互斥锁
    err = infra_mutex_create(&g_context.mutex);
    if (err != INFRA_OK) {
        infra_thread_pool_destroy(g_context.thread_pool);
        return err;
    }
    
    // 创建存储实例
    poly_memkv_config_t config_memkv = {
        .max_key_size = 256,
        .max_value_size = 1024*1024,
        .engine_type = g_context.engine,
        .plugin_path = g_context.plugin_path
    };
    err = poly_memkv_create(&config_memkv, &g_context.store);
    if (err != INFRA_OK) {
        infra_mutex_destroy(&g_context.mutex);
        infra_thread_pool_destroy(g_context.thread_pool);
        return err;
    }
    
    return INFRA_OK;
}

// 启动服务
infra_error_t memkv_start(void) {
    if (g_context.running) {
        return INFRA_ERROR_ALREADY_EXISTS;
    }
    
    g_context.running = true;
    
    // 创建服务线程
    pthread_t thread;
    int rc = pthread_create(&thread, NULL, service_thread, NULL);
    if (rc != 0) {
        g_context.running = false;
        return INFRA_ERROR_SYSTEM;
    }
    
    pthread_detach(thread);
    return INFRA_OK;
}

// 停止服务
static infra_error_t memkv_stop(void) {
    if (!g_context.running) {
        return INFRA_OK;
    }
    
    g_context.running = false;
    
    // 等待所有任务完成并销毁线程池
    if (g_context.thread_pool) {
        infra_thread_pool_destroy(g_context.thread_pool);
        g_context.thread_pool = NULL;
    }
    
    return INFRA_OK;
}

// 清理服务
static infra_error_t memkv_cleanup(void) {
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
    // 设置默认值
    g_context.engine = POLY_MEMKV_ENGINE_SQLITE;  // 默认使用SQLite
    
    // 解析命令行参数
    for (int i = 0; i < argc; i++) {
        if (strncmp(argv[i], "--port=", 7) == 0) {
            g_context.port = atoi(argv[i] + 7);
        }
        else if (strcmp(argv[i], "--status") == 0) {
            if (g_context.running) {
                INFRA_LOG_INFO("Service is running on port %d with %s engine",
                    g_context.port,
                    g_context.engine == POLY_MEMKV_ENGINE_SQLITE ? "sqlite" : "duckdb");
            } else {
                INFRA_LOG_INFO("Service is stopped");
            }
            return INFRA_OK;
        }
        else if (strcmp(argv[i], "--start") == 0) {
            return memkv_start();
        }
        else if (strcmp(argv[i], "--stop") == 0) {
            memkv_stop();
            return INFRA_OK;
        }
        else if (strncmp(argv[i], "--engine=", 9) == 0) {
            const char* engine = argv[i] + 9;
            if (strcmp(engine, "sqlite") == 0) {
                g_context.engine = POLY_MEMKV_ENGINE_SQLITE;
            } else if (strcmp(engine, "duckdb") == 0) {
                g_context.engine = POLY_MEMKV_ENGINE_DUCKDB;
            } else {
                INFRA_LOG_ERROR("Invalid engine type: %s", engine);
                return INFRA_ERROR_INVALID_PARAM;
            }
        }
        else if (strncmp(argv[i], "--plugin=", 9) == 0) {
            g_context.plugin_path = strdup(argv[i] + 9);
            if (!g_context.plugin_path) {
                return INFRA_ERROR_NO_MEMORY;
            }
        }
        else if (strncmp(argv[i], "--", 2) == 0) {
            g_context.plugin_path = strdup(argv[i]);
            if (!g_context.plugin_path) {
                return INFRA_ERROR_NO_MEMORY;
            }
        }
    }
    
    // 显示当前配置
    INFRA_LOG_INFO("Service configuration:");
    INFRA_LOG_INFO("  Port: %d", g_context.port);
    INFRA_LOG_INFO("  Engine: %s",
        g_context.engine == POLY_MEMKV_ENGINE_SQLITE ? "sqlite" : "duckdb");
    if (g_context.plugin_path) {
        INFRA_LOG_INFO("  Plugin path: %s", g_context.plugin_path);
    }
    
    return INFRA_OK;
}

