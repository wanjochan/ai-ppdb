#include "internal/peer/peer_service.h"
#include "internal/infra/infra_core.h"
#include "internal/infra/infra_net.h"
#include "internal/infra/infra_sync.h"
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
typedef struct {
    bool running;                        // 服务是否运行
    uint16_t port;                       // 监听端口
    infra_socket_t listener;             // 监听socket
    infra_thread_pool_t* pool;          // 线程池
    infra_mutex_t mutex;                 // 全局互斥锁
    poly_memkv_t* store;                // KV存储
    poly_memkv_engine_type_t engine;    // 存储引擎类型
    char* plugin_path;                  // 插件路径
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
static infra_error_t memkv_cmd_handler(int argc, char** argv);

//-----------------------------------------------------------------------------
// Command Line Options
//-----------------------------------------------------------------------------

const poly_cmd_option_t memkv_options[] = {
    {"port", "Server port", true},
    {"start", "Start the service", false},
    {"stop", "Stop the service", false},
    {"status", "Show service status", false},
    {"engine", "Storage engine (sqlite/duckdb)", true},
    {"plugin", "Plugin path for duckdb", false},
};

const int memkv_option_count = sizeof(memkv_options) / sizeof(memkv_options[0]);

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
static memkv_context_t g_context = {0};

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

// 读取命令
static infra_error_t read_command(infra_socket_t sock, char* buffer, size_t size) {
    if (!sock || !buffer || size == 0) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    size_t pos = 0;
    while (pos < size - 1) {
        size_t bytes = 0;
        infra_error_t err = infra_net_recv(sock, buffer + pos, 1, &bytes);
        if (err != INFRA_OK) {
            return err;
        }
        if (bytes != 1) {
            return INFRA_ERROR_IO;
        }

        if (buffer[pos] == '\n') {
            if (pos > 0 && buffer[pos - 1] == '\r') {
                buffer[pos - 1] = '\0';
            } else {
                buffer[pos] = '\0';
            }
            return INFRA_OK;
        }
        pos++;
    }

    return INFRA_ERROR_INVALID_STATE;
}

// 发送响应
static infra_error_t send_response(infra_socket_t sock, const char* response) {
    if (!sock || !response) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    size_t len = strlen(response);
    size_t sent = 0;
    infra_error_t err = infra_net_send(sock, response, len, &sent);
    if (err != INFRA_OK || sent != len) {
        return err != INFRA_OK ? err : INFRA_ERROR_IO;
    }

    return INFRA_OK;
}

// 处理连接
static void* handle_connection(void* arg) {
    infra_socket_t sock = (infra_socket_t)arg;
    if (!sock) {
        return NULL;
    }

    if (!g_context.store) {
        send_response(sock, "ERROR: Store not initialized\r\n");
        infra_net_close(sock);
        return NULL;
    }

    char buffer[MEMKV_BUFFER_SIZE];
    while (g_context.running) {
        // 读取命令
        infra_error_t err = read_command(sock, buffer, sizeof(buffer));
        if (err != INFRA_OK) {
            if (err != INFRA_ERROR_CLOSED) {
                INFRA_LOG_ERROR("Failed to read command: %d", err);
            }
            break;
        }

        INFRA_LOG_DEBUG("Received command: %s", buffer);

        // 解析命令
        char* cmd = strtok(buffer, " ");
        if (!cmd) {
            send_response(sock, "ERROR\r\n");
            continue;
        }

        // 处理命令
        if (strcmp(cmd, "get") == 0) {
            char* key = strtok(NULL, " ");
            if (!key) {
                send_response(sock, "ERROR\r\n");
                continue;
            }

            if (strlen(key) > MEMKV_MAX_KEY_SIZE) {
                send_response(sock, "ERROR: Key too long\r\n");
                continue;
            }

            poly_memkv_item_t* item = NULL;
            err = poly_memkv_get(g_context.store, key, &item);
            if (err != INFRA_OK || !item) {
                send_response(sock, "NOT_FOUND\r\n");
                continue;
            }

            // 发送值
            char response[32];
            snprintf(response, sizeof(response), "VALUE %zu\r\n", item->value_size);
            send_response(sock, response);
            send_response(sock, item->value);
            send_response(sock, "\r\n");
            poly_memkv_free_item(item);

        } else if (strcmp(cmd, "set") == 0) {
            char* key = strtok(NULL, " ");
            char* value = strtok(NULL, " ");
            if (!key || !value) {
                send_response(sock, "ERROR\r\n");
                continue;
            }

            size_t key_len = strlen(key);
            size_t value_len = strlen(value);

            if (key_len > MEMKV_MAX_KEY_SIZE) {
                send_response(sock, "ERROR: Key too long\r\n");
                continue;
            }

            if (value_len > MEMKV_MAX_VALUE_SIZE) {
                send_response(sock, "ERROR: Value too long\r\n");
                continue;
            }

            err = poly_memkv_set(g_context.store, key, value, value_len, 0, 0);
            if (err != INFRA_OK) {
                send_response(sock, "ERROR\r\n");
                continue;
            }

            send_response(sock, "STORED\r\n");

        } else if (strcmp(cmd, "delete") == 0) {
            char* key = strtok(NULL, " ");
            if (!key) {
                send_response(sock, "ERROR\r\n");
                continue;
            }

            if (strlen(key) > MEMKV_MAX_KEY_SIZE) {
                send_response(sock, "ERROR: Key too long\r\n");
                continue;
            }

            err = poly_memkv_delete(g_context.store, key);
            if (err != INFRA_OK) {
                send_response(sock, "NOT_FOUND\r\n");
                continue;
            }

            send_response(sock, "DELETED\r\n");

        } else if (strcmp(cmd, "stats") == 0) {
            const poly_memkv_stats_t* stats = poly_memkv_get_stats(g_context.store);
            if (!stats) {
                send_response(sock, "ERROR: Failed to get stats\r\n");
                continue;
            }

            char response[256];
            snprintf(response, sizeof(response),
                "STAT cmd_get %d\r\n"
                "STAT cmd_set %d\r\n"
                "STAT cmd_delete %d\r\n"
                "STAT curr_items %d\r\n"
                "STAT bytes %d\r\n"
                "END\r\n",
                stats->cmd_get, stats->cmd_set,
                stats->cmd_delete, stats->curr_items,
                stats->bytes);
            send_response(sock, response);

        } else if (strcmp(cmd, "engine") == 0) {
            char response[64];
            snprintf(response, sizeof(response), "ENGINE %s\r\n",
                g_context.engine == POLY_MEMKV_ENGINE_SQLITE ? "sqlite" : "duckdb");
            send_response(sock, response);

        } else {
            send_response(sock, "ERROR\r\n");
        }
    }

    infra_net_close(sock);
    return NULL;
}

//-----------------------------------------------------------------------------
// Service Implementation
//-----------------------------------------------------------------------------

static infra_error_t memkv_init(const infra_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    memset(&g_context, 0, sizeof(g_context));

    // 创建线程池
    infra_thread_pool_config_t pool_config = {
        .min_threads = MEMKV_MIN_THREADS,
        .max_threads = MEMKV_MAX_THREADS,
        .queue_size = MEMKV_MAX_THREADS * 2
    };

    infra_error_t err = infra_thread_pool_create(&pool_config, &g_context.pool);
    if (err != INFRA_OK) {
        return err;
    }

    // 创建互斥锁
    err = infra_mutex_create(&g_context.mutex);
    if (err != INFRA_OK) {
        infra_thread_pool_destroy(g_context.pool);
        return err;
    }

    // 创建KV存储
    poly_memkv_config_t store_config = {
        .initial_size = 1024*1024,
        .max_key_size = MEMKV_MAX_KEY_SIZE,
        .max_value_size = MEMKV_MAX_VALUE_SIZE,
        .engine_type = g_context.engine ? g_context.engine : POLY_MEMKV_ENGINE_SQLITE,
        .plugin_path = g_context.plugin_path
    };

    err = poly_memkv_create(&store_config, &g_context.store);
    if (err != INFRA_OK) {
        infra_mutex_destroy(&g_context.mutex);
        infra_thread_pool_destroy(g_context.pool);
        return err;
    }

    return INFRA_OK;
}

static infra_error_t memkv_cleanup(void) {
    if (g_context.running) {
        memkv_stop();
    }

    if (g_context.store) {
        poly_memkv_destroy(g_context.store);
        g_context.store = NULL;
    }

    if (g_context.pool) {
        infra_thread_pool_destroy(g_context.pool);
        g_context.pool = NULL;
    }

    if (g_context.plugin_path) {
        free(g_context.plugin_path);
        g_context.plugin_path = NULL;
    }

    infra_mutex_destroy(&g_context.mutex);

    return INFRA_OK;
}

static infra_error_t memkv_start(void) {
    if (g_context.running) {
        return INFRA_ERROR_BUSY;
    }

    if (!g_context.store) {
        INFRA_LOG_ERROR("Store not initialized");
        return INFRA_ERROR_INVALID_STATE;
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
    infra_net_addr_t addr = {
        .host = "127.0.0.1",
        .port = g_context.port
    };
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
    g_context.running = true;

    INFRA_LOG_INFO("Memkv service started on port %d with %s engine", 
        g_context.port,
        g_context.engine == POLY_MEMKV_ENGINE_SQLITE ? "sqlite" : "duckdb");

    // 主循环
    while (g_context.running) {
        // 接受新连接
        infra_socket_t client = NULL;
        infra_net_addr_t client_addr = {0};
        err = infra_net_accept(g_context.listener, &client, &client_addr);
        if (err != INFRA_OK) {
            if (err == INFRA_ERROR_WOULD_BLOCK) continue;
            INFRA_LOG_ERROR("Failed to accept connection: %d", err);
            continue;
        }

        INFRA_LOG_INFO("Accepted connection from %s:%d", 
            client_addr.host, client_addr.port);

        // 提交到线程池处理
        err = infra_thread_pool_submit(g_context.pool, handle_connection, client);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to submit connection to thread pool: %d", err);
            infra_net_close(client);
            continue;
        }
    }

    return INFRA_OK;
}

static infra_error_t memkv_stop(void) {
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

static bool memkv_is_running(void) {
    return g_context.running;
}

static infra_error_t memkv_cmd_handler(int argc, char** argv) {
    if (argc < 1) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // Initialize service
    infra_config_t config = INFRA_DEFAULT_CONFIG;
    infra_error_t err = memkv_init(&config);
    if (err != INFRA_OK) {
        INFRA_LOG_ERROR("Failed to initialize memkv service: %d", err);
        return err;
    }

    // Parse command line
    bool should_start = false;
    g_context.port = 11211;  // 默认端口
    g_context.engine = POLY_MEMKV_ENGINE_SQLITE;  // 默认使用SQLite

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--start") == 0) {
            should_start = true;
        } else if (strcmp(argv[i], "--stop") == 0) {
            return memkv_stop();
        } else if (strcmp(argv[i], "--status") == 0) {
            infra_printf("Service is %s\n", 
                memkv_is_running() ? "running" : "stopped");
            if (memkv_is_running()) {
                infra_printf("Engine: %s\n",
                    g_context.engine == POLY_MEMKV_ENGINE_SQLITE ? "sqlite" : "duckdb");
            }
            return INFRA_OK;
        } else if (strncmp(argv[i], "--port=", 7) == 0) {
            g_context.port = atoi(argv[i] + 7);
        } else if (strcmp(argv[i], "--port") == 0) {
            if (++i >= argc) {
                INFRA_LOG_ERROR("Missing port number");
                return INFRA_ERROR_INVALID_PARAM;
            }
            g_context.port = atoi(argv[i]);
        } else if (strncmp(argv[i], "--engine=", 9) == 0) {
            const char* engine = argv[i] + 9;
            if (strcmp(engine, "sqlite") == 0) {
                g_context.engine = POLY_MEMKV_ENGINE_SQLITE;
            } else if (strcmp(engine, "duckdb") == 0) {
                g_context.engine = POLY_MEMKV_ENGINE_DUCKDB;
            } else {
                INFRA_LOG_ERROR("Invalid engine type: %s", engine);
                return INFRA_ERROR_INVALID_PARAM;
            }
        } else if (strcmp(argv[i], "--engine") == 0) {
            if (++i >= argc) {
                INFRA_LOG_ERROR("Missing engine type");
                return INFRA_ERROR_INVALID_PARAM;
            }
            const char* engine = argv[i];
            if (strcmp(engine, "sqlite") == 0) {
                g_context.engine = POLY_MEMKV_ENGINE_SQLITE;
            } else if (strcmp(engine, "duckdb") == 0) {
                g_context.engine = POLY_MEMKV_ENGINE_DUCKDB;
            } else {
                INFRA_LOG_ERROR("Invalid engine type: %s", engine);
                return INFRA_ERROR_INVALID_PARAM;
            }
        } else if (strncmp(argv[i], "--plugin=", 9) == 0) {
            g_context.plugin_path = strdup(argv[i] + 9);
        } else if (strcmp(argv[i], "--plugin") == 0) {
            if (++i >= argc) {
                INFRA_LOG_ERROR("Missing plugin path");
                return INFRA_ERROR_INVALID_PARAM;
            }
            g_context.plugin_path = strdup(argv[i]);
        }
    }

    // Start service if requested
    if (should_start) {
        INFRA_LOG_DEBUG("Starting memkv service");
        err = memkv_start();
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to start memkv service: %d", err);
            return err;
        }
        INFRA_LOG_INFO("Memkv service started successfully with %s engine",
            g_context.engine == POLY_MEMKV_ENGINE_SQLITE ? "sqlite" : "duckdb");
    }

    return INFRA_OK;
}
