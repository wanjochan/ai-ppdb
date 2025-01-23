#include "internal/infra/infra_core.h"
#include "internal/peer/peer_memkv.h"

//-----------------------------------------------------------------------------
// Command Line Options
//-----------------------------------------------------------------------------

const poly_cmd_option_t memkv_options[] = {
    {"port", "Port to listen on", true},
    {"start", "Start the service", false},
    {"stop", "Stop the service", false},
    {"status", "Show service status", false},
};

const int memkv_option_count = sizeof(memkv_options) / sizeof(memkv_options[0]);

//-----------------------------------------------------------------------------
// Global Variables
//-----------------------------------------------------------------------------

static struct {
    bool running;                  // 服务运行状态
    infra_socket_t listener;       // 监听socket
    infra_thread_pool_t* pool;     // 线程池
    poly_hashtable_t* store;       // KV存储
    infra_mutex_t store_mutex;     // 存储互斥锁
    uint16_t port;                 // 监听端口
    memkv_stats_t stats;          // 统计信息
    uint64_t cas_counter;         // CAS计数器
} g_context = {0};

//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------

static void* handle_connection(void* arg);
static infra_error_t create_listener(void);
static infra_error_t process_command(memkv_conn_t* conn);
static infra_error_t parse_command(memkv_conn_t* conn);
static infra_error_t execute_command(memkv_conn_t* conn);

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

// 创建连接
static memkv_conn_t* create_connection(infra_socket_t socket) {
    memkv_conn_t* conn = calloc(1, sizeof(memkv_conn_t));
    if (!conn) {
        return NULL;
    }

    conn->socket = socket;
    conn->buffer = malloc(MEMKV_BUFFER_SIZE);
    if (!conn->buffer) {
        free(conn);
        return NULL;
    }

    conn->buffer_size = MEMKV_BUFFER_SIZE;
    conn->buffer_used = 0;
    conn->state = PARSE_STATE_INIT;

    return conn;
}

// 销毁连接
static void destroy_connection(memkv_conn_t* conn) {
    if (!conn) return;
    
    if (conn->current_cmd.key) {
        free(conn->current_cmd.key);
    }
    if (conn->current_cmd.data) {
        free(conn->current_cmd.data);
    }
    
    free(conn->buffer);
    free(conn);
}

// 创建存储项
static memkv_item_t* create_item(const char* key, const void* value, 
    size_t value_size, uint32_t flags, time_t exptime) {
    
    memkv_item_t* item = malloc(sizeof(memkv_item_t));
    if (!item) {
        return NULL;
    }

    item->key = strdup(key);
    if (!item->key) {
        free(item);
        return NULL;
    }

    item->value = malloc(value_size);
    if (!item->value) {
        free(item->key);
        free(item);
        return NULL;
    }

    memcpy(item->value, value, value_size);
    item->value_size = value_size;
    item->flags = flags;
    item->exptime = exptime ? (time(NULL) + exptime) : 0;
    item->cas = __sync_add_and_fetch(&g_context.cas_counter, 1);

    return item;
}

// 销毁存储项
static void destroy_item(memkv_item_t* item) {
    if (!item) return;
    free(item->key);
    free(item->value);
    free(item);
}

// 检查项是否过期
static bool is_item_expired(const memkv_item_t* item) {
    return item->exptime && time(NULL) > item->exptime;
}

// 更新统计信息
static void update_stats_set(size_t value_size) {
    __sync_add_and_fetch(&g_context.stats.cmd_set, 1);
    __sync_add_and_fetch(&g_context.stats.curr_items, 1);
    __sync_add_and_fetch(&g_context.stats.total_items, 1);
    __sync_add_and_fetch(&g_context.stats.bytes, value_size);
}

static void update_stats_delete(size_t value_size) {
    __sync_add_and_fetch(&g_context.stats.cmd_delete, 1);
    __sync_sub_and_fetch(&g_context.stats.curr_items, 1);
    __sync_sub_and_fetch(&g_context.stats.bytes, value_size);
}

static void update_stats_get(bool hit) {
    __sync_add_and_fetch(&g_context.stats.cmd_get, 1);
    if (hit) {
        __sync_add_and_fetch(&g_context.stats.hits, 1);
    } else {
        __sync_add_and_fetch(&g_context.stats.misses, 1);
    }
}

//-----------------------------------------------------------------------------
// Core Functions Implementation
//-----------------------------------------------------------------------------

infra_error_t memkv_init(const infra_config_t* config) {
    if (!config) {
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 初始化互斥锁
    infra_error_t err = infra_mutex_init(&g_context.store_mutex);
    if (err != INFRA_OK) {
        return err;
    }

    // 创建KV存储
    err = poly_hashtable_create(
        1024,  // 初始大小
        poly_hashtable_string_hash,
        poly_hashtable_string_compare,
        &g_context.store
    );
    if (err != INFRA_OK) {
        infra_mutex_destroy(&g_context.store_mutex);
        return err;
    }

    // 创建线程池
    err = infra_thread_pool_create(MEMKV_MIN_THREADS, MEMKV_MAX_THREADS, &g_context.pool);
    if (err != INFRA_OK) {
        poly_hashtable_destroy(g_context.store);
        infra_mutex_destroy(&g_context.store_mutex);
        return err;
    }

    // 初始化统计信息
    memset(&g_context.stats, 0, sizeof(g_context.stats));
    g_context.cas_counter = 0;

    return INFRA_OK;
}

infra_error_t memkv_cleanup(void) {
    if (g_context.running) {
        return INFRA_ERROR_BUSY;
    }

    if (g_context.store) {
        infra_mutex_lock(&g_context.store_mutex);
        poly_hashtable_foreach(g_context.store, 
            (poly_hashtable_iter_fn)destroy_item, NULL);
        poly_hashtable_destroy(g_context.store);
        g_context.store = NULL;
        infra_mutex_unlock(&g_context.store_mutex);
        infra_mutex_destroy(&g_context.store_mutex);
    }

    if (g_context.pool) {
        infra_thread_pool_destroy(g_context.pool);
        g_context.pool = NULL;
    }

    return INFRA_OK;
}

static infra_error_t create_listener(void) {
    // 创建监听socket
    infra_error_t err = infra_net_create_socket(&g_context.listener);
    if (err != INFRA_OK) {
        return err;
    }

    // 绑定端口
    err = infra_net_bind(g_context.listener, NULL, g_context.port);
    if (err != INFRA_OK) {
        infra_net_close_socket(g_context.listener);
        return err;
    }

    // 开始监听
    err = infra_net_listen(g_context.listener, 128);
    if (err != INFRA_OK) {
        infra_net_close_socket(g_context.listener);
        return err;
    }

    return INFRA_OK;
}

infra_error_t memkv_start(void) {
    if (g_context.running) {
        return INFRA_ERROR_BUSY;
    }

    // 创建监听socket
    infra_error_t err = create_listener();
    if (err != INFRA_OK) {
        return err;
    }

    g_context.running = true;

    // 主循环：接受连接
    while (g_context.running) {
        infra_socket_t client_socket;
        err = infra_net_accept(g_context.listener, &client_socket);
        
        if (!g_context.running) {
            break;
        }

        if (err != INFRA_OK) {
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                continue;
            }
            INFRA_LOG_ERROR("Accept failed: %d", err);
            continue;
        }

        // 创建连接结构
        memkv_conn_t* conn = create_connection(client_socket);
        if (!conn) {
            infra_net_close_socket(client_socket);
            continue;
        }

        // 提交给线程池处理
        err = infra_thread_pool_submit(g_context.pool, handle_connection, conn);
        if (err != INFRA_OK) {
            INFRA_LOG_ERROR("Failed to submit task: %d", err);
            destroy_connection(conn);
            infra_net_close_socket(client_socket);
        }
    }

    return INFRA_OK;
}

infra_error_t memkv_stop(void) {
    if (!g_context.running) {
        return INFRA_ERROR_NOT_RUNNING;
    }

    g_context.running = false;
    infra_net_close_socket(g_context.listener);

    return INFRA_OK;
}

bool memkv_is_running(void) {
    return g_context.running;
}

//-----------------------------------------------------------------------------
// Connection Handling
//-----------------------------------------------------------------------------

static void* handle_connection(void* arg) {
    memkv_conn_t* conn = (memkv_conn_t*)arg;
    if (!conn) return NULL;

    while (g_context.running) {
        // 读取命令
        size_t bytes_read = 0;
        infra_error_t err = infra_net_recv(
            conn->socket,
            conn->buffer + conn->buffer_used,
            MEMKV_BUFFER_SIZE - conn->buffer_used,
            &bytes_read
        );

        if (err != INFRA_OK) {
            if (err != INFRA_ERROR_WOULD_BLOCK) {
                break;
            }
            continue;
        }

        if (bytes_read == 0) {
            break;  // 连接关闭
        }

        conn->buffer_used += bytes_read;

        // 处理命令
        err = process_command(conn);
        if (err != INFRA_OK) {
            break;
        }
    }

    infra_net_close_socket(conn->socket);
    destroy_connection(conn);
    return NULL;
}

//-----------------------------------------------------------------------------
// Command Processing
//-----------------------------------------------------------------------------

static infra_error_t process_command(memkv_conn_t* conn) {
    while (conn->buffer_used > 0) {
        infra_error_t err;

        // 解析命令
        if (conn->state == PARSE_STATE_INIT) {
            err = memkv_parse_command(conn);
            if (err == INFRA_ERROR_WOULD_BLOCK) {
                return INFRA_OK;
            }
            if (err != INFRA_OK) {
                memkv_send_response(conn, "ERROR\r\n");
                return err;
            }
        }

        // 处理数据块
        if (conn->state == PARSE_STATE_DATA) {
            if (conn->buffer_used < conn->data_remaining) {
                return INFRA_OK;
            }

            // 分配数据缓冲区
            conn->current_cmd.data = malloc(conn->current_cmd.bytes);
            if (!conn->current_cmd.data) {
                memkv_send_response(conn, "SERVER_ERROR out of memory\r\n");
                return INFRA_ERROR_NO_MEMORY;
            }

            // 复制数据
            memcpy(conn->current_cmd.data, conn->buffer, 
                conn->current_cmd.bytes);

            // 检查结束标记
            if (conn->buffer[conn->current_cmd.bytes] != '\r' ||
                conn->buffer[conn->current_cmd.bytes + 1] != '\n') {
                memkv_send_response(conn, "CLIENT_ERROR bad data chunk\r\n");
                return INFRA_ERROR_INVALID_PARAM;
            }

            // 移动缓冲区
            memmove(conn->buffer, 
                conn->buffer + conn->data_remaining,
                conn->buffer_used - conn->data_remaining);
            conn->buffer_used -= conn->data_remaining;
            conn->state = PARSE_STATE_COMPLETE;
        }

        // 执行命令
        if (conn->state == PARSE_STATE_COMPLETE) {
            err = memkv_execute_command(conn);
            
            // 清理命令
            if (conn->current_cmd.key) {
                free(conn->current_cmd.key);
                conn->current_cmd.key = NULL;
            }
            if (conn->current_cmd.data) {
                free(conn->current_cmd.data);
                conn->current_cmd.data = NULL;
            }
            
            conn->state = PARSE_STATE_INIT;
            
            if (err == INFRA_ERROR_CLOSED) {
                return err;
            }
        }
    }

    return INFRA_OK;
}

//-----------------------------------------------------------------------------
// Command Handler
//-----------------------------------------------------------------------------

infra_error_t memkv_cmd_handler(int argc, char** argv) {
    if (argc < 2) {
        INFRA_LOG_ERROR("No command specified");
        return INFRA_ERROR_INVALID_PARAM;
    }

    // 解析选项
    const char* port_str = NULL;
    bool start = false;
    bool stop = false;
    bool status = false;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--port=", 7) == 0) {
            port_str = argv[i] + 7;
        } else if (strcmp(argv[i], "--start") == 0) {
            start = true;
        } else if (strcmp(argv[i], "--stop") == 0) {
            stop = true;
        } else if (strcmp(argv[i], "--status") == 0) {
            status = true;
        }
    }

    // 处理命令
    if (status) {
        infra_printf("MemKV service is %s\n", 
            memkv_is_running() ? "running" : "stopped");
        return INFRA_OK;
    }

    if (stop) {
        return memkv_stop();
    }

    if (start) {
        if (!port_str) {
            INFRA_LOG_ERROR("Port not specified");
            return INFRA_ERROR_INVALID_PARAM;
        }

        // 解析端口
        char* endptr;
        long port = strtol(port_str, &endptr, 10);
        if (*endptr != '\0' || port <= 0 || port > 65535) {
            INFRA_LOG_ERROR("Invalid port: %s", port_str);
            return INFRA_ERROR_INVALID_PARAM;
        }

        g_context.port = (uint16_t)port;
        return memkv_start();
    }

    INFRA_LOG_ERROR("Invalid command");
    return INFRA_ERROR_INVALID_PARAM;
}

const memkv_stats_t* memkv_get_stats(void) {
    return &g_context.stats;
}
